// writeback_rtl_directed.cpp -- TERRAIN.WRITEBACK against
// zref::terrain::sheet_writeback.
//
// ---------------------------------------------------------------------------
// WHAT CAN GO SILENTLY WRONG HERE
// ---------------------------------------------------------------------------
// This block's failures are quieter than the loader's, and that asymmetry
// decides what the file checks.
//
//  * A BARRIER RELEASED WITHOUT AN ACK looks exactly like a fast machine. The
//    slot loads, the page reloads from a journal entry that was never written,
//    and the player's scars have healed. Nothing faults, nothing counts. So
//    `wb_valid` is checked to be LOW across every window in which it must be --
//    for the whole transfer, for a whole unacknowledged wait, after an
//    unmatched ACK, and after a NAK.
//  * A SHEET EXTRACTED FROM THE WRONG OFFSET is 8,192 real bytes of real
//    terrain, just not this terrain. Layer F starts six bytes off a burst
//    boundary, so an off-by-one lane or an off-by-one chunk both produce a
//    plausible entry. So the four bytes on either side of the sheet's two edges
//    are corrupted one at a time: 10,693 and 18,886 must NOT reach the journal,
//    10,694 and 18,885 must.
//  * A MACHINE THAT DOES ITS WORK TWICE produces a byte-identical journal
//    entry. Both incidents of 2026-09-05 were exactly that. So the bench counts
//    guard requests, read beats, bridge bursts and write beats and requires
//    130 / 1,040 / 128 / 1,024 EXACTLY -- not "at least".
//  * A COMPLETION DROPPED because the sequencer was busy strands a job; a
//    BARRIER RELEASE dropped because the directory was busy strands a slot in
//    EVICT_PENDING forever. Both are held with `ready` low and compared against
//    themselves every cycle.
//  * A COUNTER THAT COUNTS CYCLES WHILE CLAIMING EVENTS reports the producer's
//    patience as throughput. `acks_overdue` is asserted to be exactly ONE for a
//    ticket that waits three deadlines, not the number of cycles it waited.
//
// The oracle is `zref::terrain::sheet_writeback`, which composes
// `zref::mem::upload_verdict` with the roles reversed rather than restating it
// -- so this test is also what keeps the writeback's refusal order tied to the
// console's one upload law.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vtb_writeback.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_page.hpp"

namespace tp = zref::terrain;

namespace {

int g_checks = 0;
int g_fail = 0;

void ck(bool ok, const char* what, long long want = 1, long long got = 0) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s (expected %lld, got %lld)\n", what, want, got);
  }
}

void cke(uint64_t want, uint64_t got, const char* what) {
  ck(want == got, what, static_cast<long long>(want), static_cast<long long>(got));
}

// ---------------------------------------------------------------- geometry --
constexpr uint32_t kPageWords = tp::kPageBytes / 8;   // 2,672
constexpr uint32_t kSheetWords = tp::kLayerFBytes / 8;  // 1,024

// The played page-pool image holds TWO slots, so "the neighbouring page was not
// disturbed" is a real check and not a hope.
constexpr uint32_t kVramWindow = tp::kPagePoolBase;  // 0x0400_0000
constexpr uint32_t kJnlWindow = 0x30000000u;
constexpr uint32_t kJnlEntries = 4;
constexpr uint32_t kJnlBytes = kJnlEntries * tp::kLayerFBytes;  // 32,768
constexpr uint32_t kEpoch = 0x0001BEEFu;

// Exactly what the block does: 1 header read + 129 sheet chunks, 8 beats each,
// then 128 journal bursts of 8 beats. Spelled from the ruling and the layer
// table rather than read out of the RTL, so a wrong constant in the block fails
// here instead of agreeing with itself.
constexpr uint32_t kGuardReqs = 1 + tp::kSheetReadChunks;  // 130
constexpr uint32_t kReadBeats = kGuardReqs * 8;            // 1,040
constexpr uint32_t kWriteBursts = tp::kSheetWriteBursts;   // 128
constexpr uint32_t kWriteBeats = kSheetWords;              // 1,024

// Must match tb_writeback.sv's override. A watchdog whose deadline the test
// cannot reach is a watchdog nobody has ever seen fire.
constexpr uint32_t kAckDeadline = 20000;

// T3's client. MEM.GUARD gives it a WRITE-ONLY window over TERRAIN.PAGE_POOL,
// and this block READS that pool -- see section 8.
constexpr uint32_t kTerrainBuildClient = 6;
constexpr uint32_t kOtherClients[] = {0, 1, 2, 3, 4, 5, 7};

// The pool's bounds, spelled from ruling T2 rather than from the RTL.
constexpr uint32_t kPoolBase = 0x04000000u;
constexpr uint32_t kPoolEnd = 0x054E0000u;  // half-open; T2's 0x054D_FFFF + 1

uint64_t full_be(unsigned len) { return len >= 64 ? ~0ull : ((1ull << len) - 1); }

void wr16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFF);
  p[1] = static_cast<uint8_t>(v >> 8);
}
void wr32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFF);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
  p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
  p[3] = static_cast<uint8_t>(v >> 24);
}

std::vector<uint8_t> make_page(uint32_t island, int16_t ix, int16_t iz, uint32_t seed) {
  std::vector<uint8_t> b(tp::kPageBytes, 0);
  wr16(&b[0], 1);  // format_version
  b[2] = 1;        // pitch_log2 = +1, the canonical 2.0 m
  b[3] = 0;
  wr32(&b[4], island);
  wr16(&b[8], static_cast<uint16_t>(ix));
  wr16(&b[10], static_cast<uint16_t>(iz));
  wr32(&b[12], 0xA5A50001u);
  uint32_t s = seed ? seed : 1u;
  for (uint32_t i = tp::kPageHeaderBytes; i < tp::kPageBytes; ++i) {
    s = s * 1664525u + 1013904223u;
    b[i] = static_cast<uint8_t>(s >> 24);
  }
  wr32(&b[32], tp::page_payload_crc(b.data()));
  return b;
}

// ------------------------------------------------------------------- bench --
struct Bench {
  Vtb_writeback d;
  long long cycles = 0;

  void tick() {
    zhao::tick(d);
    ++cycles;
  }

  void reset() {
    d.rst_n = 0;
    d.mw_en = 0;
    d.j_valid = 0;
    d.ack_valid = 0;
    d.wb_ready = 0;
    d.done_ready = 0;
    d.p_valid = 0;
    d.stat_clear_i = 0;
    d.cfg_vram_window_base_i = kVramWindow;
    d.cfg_hps_window_base_i = kJnlWindow;
    d.cfg_region_ok_i = 1;
    d.cfg_deny_mode_i = 0;
    d.cfg_deny_idx_i = 0;
    d.cfg_err_mode_i = 0;
    d.cfg_err_burst_i = 0;
    d.cfg_vram_client_i = kTerrainBuildClient;
    d.cfg_hps_client_i = kTerrainBuildClient;
    d.cfg_journal_base_i = kJnlWindow;
    d.cfg_journal_bytes_i = kJnlBytes;
    d.cfg_epoch_i = kEpoch;
    d.eval();
    for (int i = 0; i < 4; ++i) tick();
    d.rst_n = 1;
    tick();
  }

  void stage_page(uint32_t slot_index, const std::vector<uint8_t>& page) {
    const uint32_t base_word = slot_index * kPageWords;
    d.mw_sel = 0;
    for (uint32_t w = 0; w < kPageWords; ++w) {
      uint64_t v = 0;
      for (int k = 7; k >= 0; --k) v = (v << 8) | page[w * 8 + static_cast<uint32_t>(k)];
      d.mw_en = 1;
      d.mw_addr = static_cast<uint16_t>(base_word + w);
      d.mw_data = v;
      tick();
    }
    d.mw_en = 0;
  }

  // A distinctive filler, so a stray write is visible and an entry that was
  // never touched can be proved untouched rather than assumed.
  void wipe_journal() {
    d.mw_sel = 1;
    for (uint32_t w = 0; w < kJnlEntries * kSheetWords; ++w) {
      d.mw_en = 1;
      d.mw_addr = static_cast<uint16_t>(w);
      d.mw_data = 0xEEEEEEEEEEEEEEEEull;
      tick();
    }
    d.mw_en = 0;
  }

  uint64_t jnl_word(uint32_t idx) {
    d.mr_sel = 1;
    d.mr_addr = static_cast<uint16_t>(idx);
    tick();
    return d.mr_data;
  }

  uint64_t vram_word(uint32_t idx) {
    d.mr_sel = 0;
    d.mr_addr = static_cast<uint16_t>(idx);
    tick();
    return d.mr_data;
  }

  void clear_stats() {
    d.stat_clear_i = 1;
    d.eval();
    tick();
    d.stat_clear_i = 0;
    d.eval();
  }
};

struct Job {
  uint32_t slot = 0;
  uint8_t gen = 0;
  uint32_t epoch = kEpoch;
  uint32_t island = 0;
  int16_t ix = 0;
  int16_t iz = 0;
  uint64_t journal_addr = kJnlWindow;
  uint32_t seq = 0;
  uint32_t src_id = 0;
};

void set_timing(Bench& b, int grant_hold, int rd_latency, int rd_gap, int wr_latency,
                int wr_gap) {
  b.d.cfg_grant_hold_i = static_cast<uint8_t>(grant_hold);
  b.d.cfg_rd_latency_i = static_cast<uint8_t>(rd_latency);
  b.d.cfg_rd_gap_i = static_cast<uint8_t>(rd_gap);
  b.d.cfg_wr_latency_i = static_cast<uint8_t>(wr_latency);
  b.d.cfg_wr_gap_i = static_cast<uint8_t>(wr_gap);
}

void present(Bench& b, const Job& j) {
  b.d.j_slot = static_cast<uint16_t>(j.slot);
  b.d.j_gen = j.gen;
  b.d.j_epoch = j.epoch;
  b.d.j_island = j.island;
  b.d.j_ix = static_cast<uint32_t>(static_cast<int32_t>(j.ix));
  b.d.j_iz = static_cast<uint32_t>(static_cast<int32_t>(j.iz));
  b.d.j_journal_addr = j.journal_addr;
  b.d.j_seq = j.seq;
  b.d.j_src_id = j.src_id;
  b.d.j_valid = 1;
  b.d.eval();
}

// Push one job and wait for the engine to be done with it -- which is NOT the
// same as the job being finished. A successful sheet bumps `sheets_written` and
// then WAITS for an acknowledgement; a refused or faulted one produces a
// completion straight away. Returns true if the bytes went out.
struct Submitted {
  bool transferred = false;  // the 8,192 bytes retired on the bridge
  bool completed = false;    // a completion is standing at `done_valid`
  bool accepted = false;     // the job port took it at all
  int stall_cycles = 0;      // how long `j_ready` was low while offered
};

Submitted submit(Bench& b, const Job& j, int max_wait_ready = 64) {
  Submitted s;
  const uint32_t w0 = b.d.sheets_written;
  present(b, j);
  int spin = 0;
  while (!b.d.j_ready && spin < max_wait_ready) {
    b.tick();
    ++spin;
    b.d.eval();
  }
  s.stall_cycles = spin;
  if (!b.d.j_ready) {
    b.d.j_valid = 0;
    b.d.eval();
    return s;
  }
  s.accepted = true;
  b.tick();  // the accepting edge
  b.d.j_valid = 0;
  b.d.eval();

  long long guard = 0;
  while (true) {
    if (b.d.sheets_written != w0) {
      s.transferred = true;
      return s;
    }
    if (b.d.done_valid) {
      s.completed = true;
      return s;
    }
    b.tick();
    // 400,000 is ~36x the slowest stalled sheet this bench produces (about
    // 11,000 cycles at grant_hold 5 / rd_latency 16 / rd_gap 2 / wr_latency 7 /
    // wr_gap 3). It was 4,000,000, and under a deliberate perturbation that
    // wedges the machine the suite then spent minutes per draw discovering
    // nothing it could not have discovered in seconds -- a timeout budget that
    // makes breaking the block on purpose expensive is a timeout budget that
    // discourages the one activity that proves the suite works.
    if (++guard > 400000LL) return s;
  }
}

struct DoneRec {
  bool ok = false;
  int verdict = -1;
  uint32_t slot = 0;
  uint8_t gen = 0;
  uint32_t epoch = 0;
  uint32_t seq = 0;
  uint32_t src_id = 0;
  int held_stable = 0;
  bool timed_out = false;
};

// HELD, then taken. A completion that evaporates because the consumer was busy
// is a job the sequencer waits on forever.
DoneRec take_done(Bench& b, int hold) {
  DoneRec r;
  long long guard = 0;
  while (!b.d.done_valid) {
    b.tick();
    if (++guard > 200000LL) {
      r.timed_out = true;
      return r;
    }
  }
  const uint32_t v0 = b.d.done_verdict, s0 = b.d.done_slot, q0 = b.d.done_seq;
  const uint32_t i0 = b.d.done_src_id, o0 = b.d.done_ok;
  bool stable = true;
  for (int i = 0; i < hold; ++i) {
    b.tick();
    if (!b.d.done_valid || b.d.done_verdict != v0 || b.d.done_slot != s0 ||
        b.d.done_seq != q0 || b.d.done_src_id != i0 || b.d.done_ok != o0) {
      stable = false;
      break;
    }
  }
  r.held_stable = stable ? hold : -1;
  r.ok = b.d.done_ok != 0;
  r.verdict = static_cast<int>(b.d.done_verdict);
  r.slot = b.d.done_slot;
  r.gen = static_cast<uint8_t>(b.d.done_gen);
  r.epoch = b.d.done_epoch;
  r.seq = b.d.done_seq;
  r.src_id = b.d.done_src_id;
  b.d.done_ready = 1;
  b.d.eval();
  b.tick();
  b.d.done_ready = 0;
  b.d.eval();
  return r;
}

struct WbRec {
  uint32_t slot = 0;
  uint8_t gen = 0;
  uint32_t epoch = 0;
  int held_stable = 0;
  bool timed_out = false;
};

// The barrier release. Same discipline: a dropped `wb` strands a slot in
// EVICT_PENDING forever, which is the mirror of the dropped `fin` the loader
// guards against.
WbRec take_wb(Bench& b, int hold, long long budget = 200000LL) {
  WbRec r;
  long long guard = 0;
  while (!b.d.wb_valid) {
    b.tick();
    if (++guard > budget) {
      r.timed_out = true;
      return r;
    }
  }
  const uint32_t s0 = b.d.wb_slot, g0 = b.d.wb_gen, e0 = b.d.wb_epoch;
  bool stable = true;
  for (int i = 0; i < hold; ++i) {
    b.tick();
    if (!b.d.wb_valid || b.d.wb_slot != s0 || b.d.wb_gen != g0 || b.d.wb_epoch != e0) {
      stable = false;
      break;
    }
  }
  r.held_stable = stable ? hold : -1;
  r.slot = b.d.wb_slot;
  r.gen = static_cast<uint8_t>(b.d.wb_gen);
  r.epoch = b.d.wb_epoch;
  b.d.wb_ready = 1;
  b.d.eval();
  b.tick();
  b.d.wb_ready = 0;
  b.d.eval();
  return r;
}

void send_ack(Bench& b, uint32_t seq, bool ok) {
  b.d.ack_seq = seq;
  b.d.ack_ok = ok ? 1 : 0;
  b.d.ack_valid = 1;
  b.d.eval();
  int spin = 0;
  while (!b.d.ack_ready && spin++ < 64) b.tick();
  b.tick();
  b.d.ack_valid = 0;
  b.d.eval();
}

// True if `wb_valid` stays low for n cycles. The barrier's whole meaning.
bool wb_silent(Bench& b, int n) {
  for (int i = 0; i < n; ++i) {
    if (b.d.wb_valid) return false;
    b.tick();
  }
  return !b.d.wb_valid;
}

// ------------------------------------------------------------- the oracle --
tp::SheetWritebackResult oracle(const Job& j, const std::vector<uint8_t>* page, bool seq_dup,
                                bool complete, int ack, std::vector<uint8_t>* out,
                                tp::SheetWritebackLedger* L) {
  tp::SheetWritebackRequest r;
  r.slot = j.slot;
  r.generation = j.gen;
  r.epoch = j.epoch;
  r.island_id = j.island;
  r.patch_ix = j.ix;
  r.patch_iz = j.iz;
  r.journal_addr = j.journal_addr;
  r.seq = j.seq;
  r.src_id = j.src_id;
  const zref::mem::GuardRegion arena{kJnlWindow, kJnlBytes};
  static const std::vector<uint8_t> zero(tp::kPageBytes, 0);
  const uint8_t* p = page ? page->data() : zero.data();
  return tp::sheet_writeback(r, p, arena, tp::kPagePoolBase, tp::kPagePoolSlots, kEpoch, seq_dup,
                             complete, ack, true, out ? out->data() : nullptr, L);
}

// One request put to the REAL `zhao_mem_guard` the bench drives directly,
// answered in COUNTER DELTAS. Deltas rather than levels because the verdict bits
// are one-cycle pulses one cycle after the accept -- reading them by hand is the
// exact mistake `tools/rtl/check_guard_verdict.py` exists to catch.
struct ProbeVerdict {
  uint32_t ok = 0;
  uint32_t fwd = 0;
  uint32_t viol = 0;
};

ProbeVerdict guard_probe(Bench& b, bool write, uint32_t client, uint32_t addr, unsigned len,
                         uint64_t be) {
  const uint32_t ok0 = b.d.p_ok_count, fwd0 = b.d.p_fwd_count, viol0 = b.d.p_viol_count;
  b.d.p_write = write ? 1 : 0;
  b.d.p_client = static_cast<uint8_t>(client);
  b.d.p_addr = addr;
  b.d.p_len = static_cast<uint8_t>(len);
  b.d.p_be = be;
  b.d.p_valid = 1;
  b.d.eval();
  int spin = 0;
  while (!b.d.p_ready && spin++ < 64) b.tick();
  b.tick();
  b.d.p_valid = 0;
  b.d.eval();
  for (int i = 0; i < 4; ++i) b.tick();
  ProbeVerdict v;
  v.ok = b.d.p_ok_count - ok0;
  v.fwd = b.d.p_fwd_count - fwd0;
  v.viol = b.d.p_viol_count - viol0;
  return v;
}

void probe_admits(Bench& b, const char* what, bool write, uint32_t client, uint32_t addr,
                  unsigned len) {
  const ProbeVerdict v = guard_probe(b, write, client, addr, len, full_be(len));
  ck(v.ok == 1 && v.fwd == 1 && v.viol == 0, what, 1,
     static_cast<long long>(v.ok * 100 + v.fwd * 10 + v.viol));
}

void probe_refuses(Bench& b, const char* what, bool write, uint32_t client, uint32_t addr,
                   unsigned len) {
  const ProbeVerdict v = guard_probe(b, write, client, addr, len, full_be(len));
  ck(v.ok == 0 && v.fwd == 0 && v.viol == 1, what, 1,
     static_cast<long long>(v.ok * 100 + v.fwd * 10 + v.viol));
}

// Compare the whole journal entry against the oracle's bytes. ALL 1,024 words,
// not sampled: a realignment that is right for 1,023 of them is a realignment
// that is wrong.
int journal_mismatches(Bench& b, uint32_t entry, const std::vector<uint8_t>& want) {
  int bad = 0;
  for (uint32_t w = 0; w < kSheetWords; ++w) {
    uint64_t exp = 0;
    for (int k = 7; k >= 0; --k) exp = (exp << 8) | want[w * 8 + static_cast<uint32_t>(k)];
    if (b.jnl_word(entry * kSheetWords + w) != exp) ++bad;
  }
  return bad;
}

int journal_not_filler(Bench& b, uint32_t entry) {
  int bad = 0;
  for (uint32_t w = 0; w < kSheetWords; ++w) {
    if (b.jnl_word(entry * kSheetWords + w) != 0xEEEEEEEEEEEEEEEEull) ++bad;
  }
  return bad;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  Bench b;
  b.reset();

  tp::SheetWritebackLedger want_ledger;

  const std::vector<uint8_t> page0 = make_page(7, 3, -5, 0x1234u);
  const std::vector<uint8_t> page1 = make_page(9, -11, 40, 0x9ABCu);
  std::vector<uint8_t> sheet0(tp::kLayerFBytes, 0);
  tp::sheet_extract(page0.data(), sheet0.data());

  // =========================================================================
  // 0. THE DERIVATION ITSELF
  // -------------------------------------------------------------------------
  // The layer offset table did not exist in the tree until this block needed
  // it, so it is checked against the spec's own numbers before anything is
  // built on it. A wrong offset here would produce a perfectly self-consistent
  // suite that journals the wrong 8,192 bytes.
  // =========================================================================
  cke(10694, tp::kLayerFOff, "layer F starts at page byte 10,694");
  cke(8192, tp::kLayerFBytes, "layer F is 8,192 bytes (64x64 x {tag, strength})");
  cke(18886, tp::kLayerFOff + tp::kLayerFBytes, "...and ends at 18,886");
  cke(21320, tp::kLayerHOff + tp::kLayerHBytes, "the layer table sums to the spec's own total");
  cke(10688, tp::kSheetChunkStart, "the aligned superset starts at 10,688");
  cke(6, tp::kSheetLane, "the realignment lane is six bytes");
  cke(130, kGuardReqs, "1 header read + 129 sheet chunks");
  cke(128, kWriteBursts, "128 journal bursts");

  // =========================================================================
  // 1. THE GOLDEN SHEET, UNSTALLED
  // =========================================================================
  b.stage_page(0, page0);
  b.stage_page(1, page1);
  b.wipe_journal();
  set_timing(b, 0, 0, 0, 0, 0);
  b.clear_stats();

  Job gold;
  gold.slot = 0;
  gold.gen = 0x5A;
  gold.island = 7;
  gold.ix = 3;
  gold.iz = -5;
  gold.journal_addr = tp::sheet_journal_addr(1, kJnlWindow);
  gold.seq = 0x1001;
  gold.src_id = 0xCAFE0001u;

  Submitted s1 = submit(b, gold);
  ck(s1.transferred, "golden: the 8,192 bytes retired on the bridge");

  // THE BARRIER, BEFORE THE ACK. Nothing may be released yet.
  ck(wb_silent(b, 200), "golden: wb_valid is LOW for 200 cycles with no acknowledgement");
  cke(0, b.d.done_valid, "golden: no completion either -- the job is not finished");
  cke(1, b.d.outstanding_hwm, "golden: exactly one ticket outstanding");

  // THE HOW-MANY-TIMES HALF.
  cke(kGuardReqs, b.d.greqs_seen, "golden: exactly 130 guard requests");
  cke(kReadBeats, b.d.rbeats_seen, "golden: exactly 1,040 read beats");
  cke(kWriteBursts, b.d.bursts_seen, "golden: exactly 128 bridge write bursts");
  cke(kWriteBeats, b.d.wbeats_seen, "golden: exactly 1,024 write beats");
  cke(0, b.d.wlast_bad, "golden: wlast is exactly the 8th beat of every burst");
  cke(0, b.d.jnl_oob, "golden: no write beat left the journal image");

  // THE ADDRESSES.
  cke(tp::page_vram_addr(0), b.d.first_rd_addr, "golden: the first read is the page header");
  cke(tp::page_vram_addr(0) + tp::kSheetChunkStart + (tp::kSheetReadChunks - 1) * 64,
      b.d.last_rd_addr, "golden: the last read is the 129th sheet chunk");
  cke(static_cast<uint32_t>(gold.journal_addr), b.d.first_wr_addr,
      "golden: the first journal write is the entry base");
  cke(static_cast<uint32_t>(gold.journal_addr) + tp::kLayerFBytes - 8, b.d.last_wr_addr,
      "golden: the last journal write is the entry's final beat");

  // THE PAYLOAD, ALL OF IT.
  cke(0, journal_mismatches(b, 1, sheet0), "golden: all 1,024 journal words match the oracle");
  cke(0, journal_not_filler(b, 0), "golden: journal entry 0 was not touched");
  cke(0, journal_not_filler(b, 2), "golden: journal entry 2 was not touched");
  cke(0, journal_not_filler(b, 3), "golden: journal entry 3 was not touched");

  // The page pool is a READ source and must come back unchanged.
  {
    int bad = 0;
    for (uint32_t w = 0; w < kPageWords; ++w) {
      uint64_t exp = 0;
      for (int k = 7; k >= 0; --k)
        exp = (exp << 8) | page1[w * 8 + static_cast<uint32_t>(k)];
      if (b.vram_word(kPageWords + w) != exp) ++bad;
    }
    cke(0, bad, "golden: the neighbouring page slot is byte-identical");
  }

  // NOW THE ACKNOWLEDGEMENT.
  send_ack(b, gold.seq, true);
  WbRec w1 = take_wb(b, 40);
  ck(!w1.timed_out, "golden: the acknowledgement releases the barrier");
  cke(gold.slot, w1.slot, "golden: wb names the job's slot");
  cke(gold.gen, w1.gen, "golden: wb names the job's generation");
  cke(gold.epoch, w1.epoch, "golden: wb names the job's epoch");
  cke(40, static_cast<uint64_t>(w1.held_stable),
      "golden: wb held stable for 40 stalled cycles");

  DoneRec d1 = take_done(b, 30);
  ck(!d1.timed_out, "golden: the completion follows the release");
  cke(0, static_cast<uint64_t>(d1.verdict), "golden: verdict kSheetOk");
  ck(d1.ok, "golden: done.ok");
  cke(gold.seq, d1.seq, "golden: the completion carries the job's seq");
  cke(gold.src_id, d1.src_id, "golden: the completion carries the job's source id");
  cke(30, static_cast<uint64_t>(d1.held_stable),
      "golden: done held stable for 30 stalled cycles");

  cke(1, b.d.sheets_written, "golden: sheets_written");
  cke(1, b.d.acks_ok, "golden: acks_ok");
  cke(0, b.d.acks_nak, "golden: acks_nak stays zero");
  cke(0, b.d.acks_unmatched, "golden: acks_unmatched stays zero");
  cke(tp::kLayerFBytes, b.d.wb_bytes, "golden: wb_bytes is exactly one sheet");
  cke(0, b.d.guard_denied, "golden: ZERO guard denials on a clean sheet");
  cke(0, b.d.bridge_errs, "golden: zero bridge errors");
  cke(0, b.d.sheets_refused, "golden: nothing refused");
  cke(0, b.d.sheets_faulted, "golden: nothing faulted");

  {
    std::vector<uint8_t> got(tp::kLayerFBytes, 0);
    tp::SheetWritebackResult o = oracle(gold, &page0, false, true, 1, &got, &want_ledger);
    cke(0, static_cast<uint64_t>(o.verdict), "golden: the oracle agrees on the verdict");
    ck(o.wb_released, "golden: the oracle agrees a barrier was released");
    cke(kGuardReqs, o.guard_reqs, "golden: the oracle agrees on 130 guard requests");
    cke(0, std::memcmp(got.data(), sheet0.data(), tp::kLayerFBytes),
        "golden: the oracle's own extraction agrees byte for byte");
  }

  // THE REAL GUARD, WATCHING. It has a WRITE-ONLY terrain window and this block
  // reads, so it must have refused every one of those 130 requests. When the
  // read arm lands these three checks invert, and that inversion IS the
  // acceptance test for the amendment.
  cke(kGuardReqs, b.d.shadow_req_count, "amendment: the real guard saw all 130 requests");
  cke(0, b.d.shadow_ok_count, "amendment: the real guard passed NONE of them today");
  cke(0, b.d.shadow_fwd_count, "amendment: and forwarded none");
  cke(kGuardReqs, b.d.shadow_viol_count, "amendment: it refused all 130, loudly");

  // =========================================================================
  // 2. THE SAME SHEET WITH EVERY STALL ENGAGED
  // -------------------------------------------------------------------------
  // A sibling block passed 21 checks over every input it had and still dropped
  // answers, because every phase held the consumer's ready high.
  // =========================================================================
  b.wipe_journal();
  set_timing(b, 5, 16, 2, 7, 3);
  b.clear_stats();
  Job stalled = gold;
  stalled.seq = 0x1002;
  stalled.src_id = 0xCAFE0002u;
  stalled.journal_addr = tp::sheet_journal_addr(2, kJnlWindow);
  Submitted s2 = submit(b, stalled);
  ck(s2.transferred, "stalled: the sheet still went out");
  cke(kGuardReqs, b.d.greqs_seen, "stalled: still exactly 130 guard requests");
  cke(kWriteBursts, b.d.bursts_seen, "stalled: still exactly 128 bridge bursts");
  cke(kWriteBeats, b.d.wbeats_seen, "stalled: still exactly 1,024 write beats");
  cke(0, b.d.wlast_bad, "stalled: wlast still exactly on beat 7");
  cke(0, journal_mismatches(b, 2, sheet0), "stalled: byte-identical journal entry");
  send_ack(b, stalled.seq, true);
  WbRec w2 = take_wb(b, 60);
  cke(60, static_cast<uint64_t>(w2.held_stable), "stalled: wb held stable for 60 cycles");
  DoneRec d2 = take_done(b, 0);
  cke(0, static_cast<uint64_t>(d2.verdict), "stalled: verdict kSheetOk");
  cke(stalled.seq, d2.seq, "stalled: the completion carries the job's seq");
  oracle(stalled, &page0, false, true, 1, nullptr, &want_ledger);
  set_timing(b, 0, 0, 0, 0, 0);

  // =========================================================================
  // 3. THE ALIGNMENT BOUNDARY, AT THE BYTE
  // -------------------------------------------------------------------------
  // Layer F starts six bytes off a burst boundary, so the block reads an
  // ALIGNED SUPERSET and realigns. That means bytes 10,688..10,693 and
  // 18,886..18,943 are READ and must never be journalled, and 10,694 and 18,885
  // must be. Corrupting one byte on each side of each edge is the only test
  // that can tell a correct lane from an off-by-one one.
  // =========================================================================
  struct EdgeCase {
    uint32_t byte_index;
    bool should_change;
    const char* what;
  };
  const EdgeCase edges[] = {
      {tp::kLayerFOff - 1, false, "edge: page byte 10,693 (last of layer E) must NOT reach the journal"},
      {tp::kLayerFOff, true, "edge: page byte 10,694 (first of layer F) MUST reach it"},
      {tp::kLayerFOff + tp::kLayerFBytes - 1, true,
       "edge: page byte 18,885 (last of layer F) MUST reach it"},
      {tp::kLayerFOff + tp::kLayerFBytes, false,
       "edge: page byte 18,886 (first of layer G) must NOT reach it"},
      {tp::kSheetChunkStart, false,
       "edge: page byte 10,688 (read, but before the sheet) must NOT reach it"},
  };
  uint32_t edge_seq = 0x2000;
  for (const EdgeCase& e : edges) {
    std::vector<uint8_t> p = page0;
    p[e.byte_index] = static_cast<uint8_t>(p[e.byte_index] ^ 0xFF);
    b.stage_page(0, p);
    b.wipe_journal();
    Job je = gold;
    je.seq = ++edge_seq;
    je.src_id = 0xE0000000u | e.byte_index;
    je.journal_addr = tp::sheet_journal_addr(0, kJnlWindow);
    Submitted se = submit(b, je);
    ck(se.transferred, "edge: the sheet went out");
    const int diff = journal_mismatches(b, 0, sheet0);
    if (e.should_change) {
      ck(diff == 1, e.what, 1, diff);
    } else {
      ck(diff == 0, e.what, 0, diff);
    }
    send_ack(b, je.seq, true);
    take_wb(b, 0);
    take_done(b, 0);
    oracle(je, &p, false, true, 1, nullptr, &want_ledger);
  }
  b.stage_page(0, page0);

  // =========================================================================
  // 4. REFUSALS BEFORE ANYTHING IS TOUCHED
  // -------------------------------------------------------------------------
  // Each asserts the verdict, oracle agreement, ZERO guard requests, ZERO
  // bridge bursts, the counter, the trace, and that no barrier was released.
  // =========================================================================
  struct RefuseCase {
    Job j;
    int verdict;
    const char* what;
  };
  std::vector<RefuseCase> refusals;
  {
    Job r = gold;
    r.slot = 1024;  // one past the pool; SLOTW is 11 bits so it ARRIVES as 1024
    r.seq = 0x3001;
    r.src_id = 0xBAD00001u;
    refusals.push_back({r, tp::kSheetOutsidePool, "refuse: slot 1024 is outside the pool"});
  }
  {
    Job r = gold;
    r.journal_addr = tp::sheet_journal_addr(1, kJnlWindow) + 8;
    r.seq = 0x3002;
    r.src_id = 0xBAD00002u;
    refusals.push_back({r, tp::kSheetUnaligned, "refuse: an unaligned journal address"});
  }
  {
    Job r = gold;
    r.journal_addr = 0x0000000100000000ull | kJnlWindow;
    r.seq = 0x3003;
    r.src_id = 0xBAD00003u;
    refusals.push_back({r, tp::kSheetUnreachable, "refuse: a journal address above 32 bits"});
  }
  {
    Job r = gold;
    r.journal_addr = kJnlWindow - 64;
    r.seq = 0x3004;
    r.src_id = 0xBAD00004u;
    refusals.push_back({r, tp::kSheetOutsideJournal, "refuse: below the journal arena"});
  }
  {
    Job r = gold;
    r.journal_addr = kJnlWindow + kJnlBytes - 64;  // the 8,192-byte tail leaves
    r.seq = 0x3005;
    r.src_id = 0xBAD00005u;
    refusals.push_back({r, tp::kSheetOutsideJournal, "refuse: the sheet's tail leaves the arena"});
  }
  {
    Job r = gold;
    r.epoch = kEpoch + 1;
    r.seq = 0x3006;
    r.src_id = 0xBAD00006u;
    refusals.push_back({r, tp::kSheetEpochStale, "refuse: a stale epoch"});
  }
  {
    // ORDER IS PART OF THE LAW. Everything is wrong at once; the slot is
    // reported, because a producer bug behind an epoch that happened to close
    // must not be hidden by it.
    Job r = gold;
    r.slot = 2000;
    r.journal_addr = kJnlWindow + 8;
    r.epoch = kEpoch + 9;
    r.seq = 0x3007;
    r.src_id = 0xBAD00007u;
    refusals.push_back({r, tp::kSheetOutsidePool, "refuse: all wrong at once -- the slot first"});
  }

  b.wipe_journal();
  for (const RefuseCase& rc : refusals) {
    b.clear_stats();
    Submitted sr = submit(b, rc.j);
    ck(sr.completed && !sr.transferred, "refuse: refused without transferring");
    DoneRec dr = take_done(b, 3);
    cke(static_cast<uint64_t>(rc.verdict), static_cast<uint64_t>(dr.verdict), rc.what);
    ck(!dr.ok, "refuse: done.ok is false");
    cke(rc.j.src_id, dr.src_id, "refuse: the completion names the source id");
    cke(rc.j.seq, dr.seq, "refuse: the completion names the seq");
    cke(0, b.d.greqs_seen, "refuse: ZERO guard requests");
    cke(0, b.d.bursts_seen, "refuse: ZERO bridge bursts");
    cke(rc.j.src_id, b.d.fault_src_id, "refuse: the trace names the source id");
    cke(static_cast<uint64_t>(rc.verdict), b.d.fault_verdict, "refuse: the trace names the verdict");
    cke(rc.j.island, b.d.fault_island, "refuse: the trace names the island");
    ck(wb_silent(b, 20), "refuse: no barrier released");
    tp::SheetWritebackResult o = oracle(rc.j, &page0, false, true, 1, nullptr, &want_ledger);
    cke(static_cast<uint64_t>(rc.verdict), static_cast<uint64_t>(o.verdict),
        "refuse: the oracle agrees");
  }
  cke(refusals.size(), b.d.sheets_refused, "refuse: sheets_refused counted every one");
  cke(0, journal_not_filler(b, 1), "refuse: not one journal byte moved");

  // =========================================================================
  // 5. A VALID PAGE OF ANOTHER PATCH
  // -------------------------------------------------------------------------
  // The guard read arm this block asks for admits reads of the WHOLE pool, so
  // "journalled another patch's scars" becomes reachable and NOTHING in
  // MEM.GUARD can see it. The header restates the key; this is that redundancy
  // being spent on exactly the failure the amendment creates.
  // =========================================================================
  b.stage_page(0, page1);  // slot 0 now holds island 9's page
  b.wipe_journal();
  {
    b.clear_stats();
    Job ji = gold;
    ji.seq = 0x4001;
    ji.src_id = 0xDEAD0001u;
    Submitted si = submit(b, ji);
    ck(si.completed && !si.transferred, "ident: refused, nothing transferred");
    DoneRec di = take_done(b, 0);
    cke(tp::kSheetHeaderIdent, static_cast<uint64_t>(di.verdict),
        "ident: a valid page of the WRONG patch is refused");
    cke(1, b.d.greqs_seen, "ident: EXACTLY ONE guard request -- the header, and nothing else");
    cke(0, b.d.bursts_seen, "ident: zero bridge bursts");
    cke(0, journal_not_filler(b, 1), "ident: ZERO journal bytes written");
    cke(1, b.d.hdr_ident_fails, "ident: hdr_ident_fails counted it");
    cke(ji.src_id, b.d.fault_src_id, "ident: the trace names the source id");
    ck(wb_silent(b, 20), "ident: no barrier released");
    oracle(ji, &page1, false, true, 1, nullptr, &want_ledger);
  }
  {
    // A page of the right patch with the wrong format version is also refused,
    // and by the same arm: version is part of the identity.
    std::vector<uint8_t> pv = page0;
    wr16(&pv[0], 2);
    b.stage_page(0, pv);
    b.clear_stats();
    Job jv = gold;
    jv.seq = 0x4002;
    jv.src_id = 0xDEAD0002u;
    submit(b, jv);
    DoneRec dv = take_done(b, 0);
    cke(tp::kSheetHeaderIdent, static_cast<uint64_t>(dv.verdict),
        "ident: a wrong format_version is refused too");
    oracle(jv, &pv, false, true, 1, nullptr, &want_ledger);
  }
  b.stage_page(0, page0);

  // =========================================================================
  // 6. TRANSFERS THAT STOP PART WAY
  // -------------------------------------------------------------------------
  // Each is faulted, counted, traced, releases NOTHING, and is followed by a
  // good sheet: an abort is not a wedge.
  // =========================================================================
  struct StopCase {
    int deny_mode;
    int deny_idx;
    int err_mode;
    int err_burst;
    bool expect_guard_denial;
    const char* what;
  };
  const StopCase stops[] = {
      {1, 0, 0, 0, true, "stop: a guard denial on the page header"},
      {1, 6, 0, 0, true, "stop: a guard denial on the sixth sheet chunk"},
      {0, 0, 1, 3, false, "stop: a bridge err against write burst 3"},
      {0, 0, 2, 3, false, "stop: a bridge err mid-beat of write burst 3"},
  };
  uint32_t stop_seq = 0x5000;
  for (const StopCase& sc : stops) {
    b.wipe_journal();
    b.clear_stats();
    const uint32_t gd0 = b.d.guard_denied, be0 = b.d.bridge_errs;
    b.d.cfg_deny_mode_i = static_cast<uint8_t>(sc.deny_mode);
    b.d.cfg_deny_idx_i = static_cast<uint16_t>(sc.deny_idx);
    b.d.cfg_err_mode_i = static_cast<uint8_t>(sc.err_mode);
    b.d.cfg_err_burst_i = static_cast<uint16_t>(sc.err_burst);
    b.d.eval();
    Job js = gold;
    js.seq = ++stop_seq;
    js.src_id = 0xF0000000u | stop_seq;
    Submitted ss = submit(b, js);
    ck(ss.completed && !ss.transferred, "stop: the sheet did NOT complete its transfer");
    DoneRec ds = take_done(b, 0);
    cke(tp::kSheetIncomplete, static_cast<uint64_t>(ds.verdict), sc.what);
    ck(!ds.ok, "stop: done.ok is false");
    cke(js.src_id, b.d.fault_src_id, "stop: the trace names the source id");
    if (sc.expect_guard_denial) {
      cke(gd0 + 1, b.d.guard_denied, "stop: guard_denied counted it");
    } else {
      cke(be0 + 1, b.d.bridge_errs, "stop: bridge_errs counted it");
    }
    ck(wb_silent(b, 20), "stop: NO barrier released for a half-written sheet");
    b.d.cfg_deny_mode_i = 0;
    b.d.cfg_err_mode_i = 0;
    b.d.eval();
    oracle(js, &page0, false, false, -1, nullptr, &want_ledger);
  }
  // ...and a good sheet immediately afterwards.
  {
    b.wipe_journal();
    b.clear_stats();
    Job jg = gold;
    jg.seq = 0x5100;
    jg.src_id = 0x600D0001u;
    Submitted sg = submit(b, jg);
    ck(sg.transferred, "stop: a good sheet goes out right after four aborts");
    cke(0, journal_mismatches(b, 1, sheet0), "stop: and it is byte-correct");
    send_ack(b, jg.seq, true);
    take_wb(b, 0);
    DoneRec dg = take_done(b, 0);
    cke(0, static_cast<uint64_t>(dg.verdict), "stop: verdict kSheetOk after the aborts");
    oracle(jg, &page0, false, true, 1, nullptr, &want_ledger);
  }

  // =========================================================================
  // 7. THE ACK BARRIER -- one section per failure mode
  // =========================================================================

  // 7a. AN ACK FOR A PAGE NOBODY SENT.
  {
    b.clear_stats();
    const uint32_t um0 = b.d.acks_unmatched;
    send_ack(b, 0xDEADBEEFu, true);
    for (int i = 0; i < 8; ++i) b.tick();
    cke(um0 + 1, b.d.acks_unmatched, "ack: an unmatched sequence is counted");
    ck(wb_silent(b, 30), "ack: and releases NOTHING -- the whole point of matching on identity");
    cke(0, b.d.done_valid, "ack: an unmatched ACK produces no completion either");
  }

  // 7a'. THE SAME, BUT WITH A TICKET ACTUALLY OUTSTANDING -- which is the only
  // version that can catch the dangerous bug.
  //
  // The case above runs with an EMPTY table, so a block that matched ANY
  // outstanding ticket instead of the right one passes it: there is nothing to
  // mis-match. That perturbation was applied and this file caught it only in the
  // random phase and the ledger, whose loudest line was "every draw agreed with
  // the oracle (expected 0, got 230)" -- a number nobody would read as "an
  // acknowledgement for one page released a different page's slot". So the
  // directed version now holds a real ticket while the stranger's ACK arrives.
  {
    b.wipe_journal();
    b.clear_stats();
    const uint32_t um0 = b.d.acks_unmatched;
    Job jt = gold;
    jt.seq = 0x6100;
    jt.src_id = 0x66660001u;
    Submitted st = submit(b, jt);
    ck(st.transferred, "ack-live: a real ticket is outstanding");
    send_ack(b, jt.seq ^ 0xFFFFu, true);  // a stranger's sequence
    for (int i = 0; i < 8; ++i) b.tick();
    cke(um0 + 1, b.d.acks_unmatched, "ack-live: the stranger's ACK is unmatched");
    ck(wb_silent(b, 40),
       "ack-live: and it does NOT release the ticket that happened to be waiting");
    cke(0, b.d.acks_ok - want_ledger.acks_ok, "ack-live: nor is it counted as a good ACK");
    // The right one still works.
    send_ack(b, jt.seq, true);
    WbRec wt = take_wb(b, 0);
    ck(!wt.timed_out, "ack-live: the ticket's OWN sequence still releases it");
    cke(jt.slot, wt.slot, "ack-live: and names its slot");
    DoneRec dt = take_done(b, 0);
    cke(jt.seq, dt.seq, "ack-live: the completion names the ticket's own seq");
    oracle(jt, &page0, false, true, 1, nullptr, &want_ledger);
  }

  // 7b. AN ACK THAT NEVER COMES, AND THE WATCHDOG THAT ONLY REPORTS.
  {
    b.wipe_journal();
    b.clear_stats();
    const uint32_t ov0 = b.d.acks_overdue;
    Job jw = gold;
    jw.seq = 0x6001;
    jw.src_id = 0x11110001u;
    Submitted sw = submit(b, jw);
    ck(sw.transferred, "overdue: the sheet went out");
    for (uint32_t i = 0; i < kAckDeadline + 200; ++i) b.tick();
    cke(ov0 + 1, b.d.acks_overdue, "overdue: counted ONCE -- an event, not a cycle count");
    ck(wb_silent(b, 50), "overdue: the slot is STILL held; the watchdog does not release it");
    cke(0, b.d.done_valid, "overdue: and the job is still not finished");
    // ...and a late acknowledgement still works.
    send_ack(b, jw.seq, true);
    WbRec ww = take_wb(b, 0);
    ck(!ww.timed_out, "overdue: a late acknowledgement still releases the barrier");
    cke(jw.slot, ww.slot, "overdue: and names the right slot");
    ck(b.d.ack_wait_max_cycles > kAckDeadline,
       "overdue: ack_wait_max_cycles recorded the wait, in CYCLES", 1,
       b.d.ack_wait_max_cycles);
    take_done(b, 0);
    oracle(jw, &page0, false, true, 1, nullptr, &want_ledger);
  }

  // 7c. THE JOURNAL REFUSES THE SHEET.
  {
    b.wipe_journal();
    b.clear_stats();
    const uint32_t nak0 = b.d.acks_nak;
    Job jn = gold;
    jn.seq = 0x6002;
    jn.src_id = 0x22220002u;
    Submitted sn = submit(b, jn);
    ck(sn.transferred, "nak: the bytes went out");
    send_ack(b, jn.seq, false);
    cke(nak0 + 1, b.d.acks_nak, "nak: counted");
    DoneRec dn = take_done(b, 0);
    cke(tp::kSheetJournalNak, static_cast<uint64_t>(dn.verdict), "nak: verdict kSheetJournalNak");
    ck(!dn.ok, "nak: done.ok is false");
    cke(jn.seq, dn.seq, "nak: the completion names the seq");
    cke(tp::kSheetJournalNak, b.d.fault_verdict, "nak: the trace names the verdict");
    cke(jn.src_id, b.d.fault_src_id, "nak: the trace names the source id");
    ck(wb_silent(b, 30), "nak: NO barrier released -- the scars are not safe");
    oracle(jn, &page0, false, true, 0, nullptr, &want_ledger);
  }

  // 7d. A DUPLICATE ACK AFTER RETIREMENT.
  {
    b.clear_stats();
    const uint32_t um0 = b.d.acks_unmatched;
    send_ack(b, 0x6002, true);  // the ticket retired in 7c
    for (int i = 0; i < 8; ++i) b.tick();
    cke(um0 + 1, b.d.acks_unmatched, "ack: a SECOND ACK for a retired ticket is unmatched");
    ck(wb_silent(b, 30), "ack: and cannot resurrect a barrier release");
  }

  // 7e. AN ACK THAT OUTLIVED ITS EPOCH.
  {
    b.wipe_journal();
    b.clear_stats();
    const uint32_t ae0 = b.d.acks_after_epoch;
    Job je = gold;
    je.seq = 0x6003;
    je.src_id = 0x33330003u;
    Submitted se = submit(b, je);
    ck(se.transferred, "epoch: the sheet went out under the old epoch");
    // TerrainEpoch BEGIN installs a strictly newer epoch (T11) while the
    // writeback is still outstanding.
    b.d.cfg_epoch_i = kEpoch + 1;
    b.d.eval();
    b.tick();
    send_ack(b, je.seq, true);
    cke(ae0 + 1, b.d.acks_after_epoch, "epoch: the late ACK is counted as after-epoch");
    WbRec we = take_wb(b, 0);
    ck(!we.timed_out, "epoch: it is DELIVERED anyway -- the directory owns that judgement");
    cke(kEpoch, we.epoch, "epoch: with the TICKET's epoch, never the live one");
    cke(je.gen, we.gen, "epoch: and the ticket's generation");
    DoneRec de = take_done(b, 0);
    cke(kEpoch, de.epoch, "epoch: the completion carries the ticket's epoch too");
    b.d.cfg_epoch_i = kEpoch;
    b.d.eval();
    oracle(je, &page0, false, true, 1, nullptr, &want_ledger);
  }

  // 7f. EVICTION PRESSURE: fill the ticket table and watch the job port close.
  {
    b.wipe_journal();
    b.clear_stats();
    const uint32_t st0 = b.d.jobs_stall_cycles;
    std::vector<uint32_t> seqs;
    for (uint32_t k = 0; k < 4; ++k) {
      Job jf = gold;
      jf.slot = 0;
      jf.seq = 0x7000 + k;
      jf.src_id = 0x44440000u + k;
      jf.journal_addr = tp::sheet_journal_addr(k % kJnlEntries, kJnlWindow);
      Submitted sf = submit(b, jf);
      ck(sf.transferred, "pressure: a sheet went out with earlier ones unacknowledged");
      seqs.push_back(jf.seq);
      oracle(jf, &page0, false, true, 1, nullptr, &want_ledger);
    }
    // THE HIGH-WATER MARK IS A LEVEL, AND A LEVEL LAGS ITS OWN CAUSE BY A
    // CYCLE. `outstanding_now` is sampled on the edge that allocates the fourth
    // ticket, so it still reads three; the next edge reads four. That is
    // correct for a level monitor and the wrong thing to "fix" in the RTL --
    // the bench simply has to look one cycle later. It was found by this check
    // failing with `expected 4, got 3`.
    for (int i = 0; i < 4; ++i) b.tick();
    cke(4, b.d.outstanding_hwm, "pressure: four tickets outstanding at once");
    // The fifth job cannot be taken: `j_ready` is LOW and the offer is counted
    // in CYCLES. It is never dropped and never displaces a ticket.
    Job j5 = gold;
    j5.seq = 0x7004;
    j5.src_id = 0x44440004u;
    present(b, j5);
    cke(0, b.d.j_ready, "pressure: j_ready is LOW with the ticket table full");
    for (int i = 0; i < 50; ++i) b.tick();
    cke(0, b.d.j_ready, "pressure: and stays low");
    ck(b.d.jobs_stall_cycles >= st0 + 50, "pressure: jobs_stall_cycles counted the CYCLES", 50,
       b.d.jobs_stall_cycles - st0);
    b.d.j_valid = 0;
    b.d.eval();
    // Drain in order. Retirement is by ticket index, which is allocation order.
    for (uint32_t k = 0; k < 4; ++k) {
      send_ack(b, seqs[k], true);
      WbRec wf = take_wb(b, 0);
      ck(!wf.timed_out, "pressure: each acknowledged ticket releases in turn");
      DoneRec df = take_done(b, 0);
      cke(seqs[k], df.seq, "pressure: tickets retire in allocation order");
    }
    cke(0, b.d.wb_valid, "pressure: nothing left holding a barrier");
    // The port reopens.
    present(b, j5);
    b.d.eval();
    cke(1, b.d.j_ready, "pressure: the job port reopens once a ticket frees");
    b.d.j_valid = 0;
    b.d.eval();
  }

  // 7g. A DUPLICATE SEQUENCE IS REFUSED BEFORE ANY BYTE MOVES.
  {
    b.wipe_journal();
    b.clear_stats();
    Job ja = gold;
    ja.seq = 0x8001;
    ja.src_id = 0x55550001u;
    Submitted sa = submit(b, ja);
    ck(sa.transferred, "seq: the first sheet with this sequence went out");
    b.clear_stats();
    Job jb2 = gold;
    jb2.seq = 0x8001;  // the SAME sequence, still outstanding
    jb2.src_id = 0x55550002u;
    Submitted sb2 = submit(b, jb2);
    ck(sb2.completed && !sb2.transferred, "seq: the duplicate did not transfer");
    DoneRec db = take_done(b, 0);
    cke(tp::kSheetSeqInFlight, static_cast<uint64_t>(db.verdict),
        "seq: an in-flight duplicate sequence is refused");
    cke(0, b.d.greqs_seen, "seq: ZERO guard requests for the duplicate");
    cke(1, b.d.seq_conflicts, "seq: seq_conflicts counted it");
    cke(jb2.src_id, b.d.fault_src_id, "seq: the trace names the SECOND job's source id");
    oracle(jb2, &page0, true, true, 1, nullptr, &want_ledger);
    // The first one still finishes normally.
    send_ack(b, ja.seq, true);
    take_wb(b, 0);
    DoneRec da = take_done(b, 0);
    cke(ja.src_id, da.src_id, "seq: the original ticket is untouched by the refusal");
    oracle(ja, &page0, false, true, 1, nullptr, &want_ledger);
  }

  // =========================================================================
  // 8. THE REAL MEM.GUARD, ASKED DIRECTLY
  // -------------------------------------------------------------------------
  // The observer only ever sees the requests this block makes. This asks the
  // real block about the ones it does not -- and about the one it DOES, which
  // the guard refuses today. That refusal is the amendment's evidence and this
  // is where it inverts when the arm lands.
  // =========================================================================
  {
    const uint32_t slot_addr = tp::page_vram_addr(3);
    probe_refuses(b, "guard: TERRAIN_BUILD READING the page pool is REFUSED today", false,
                  kTerrainBuildClient, slot_addr + tp::kSheetChunkStart, 64);
    probe_refuses(b, "guard: ...and so is the page header read", false, kTerrainBuildClient,
                  slot_addr, 64);
    probe_admits(b, "guard: TERRAIN_BUILD WRITING the pool still passes (the loader's arm)",
                 true, kTerrainBuildClient, slot_addr, 64);
    for (uint32_t c : kOtherClients) {
      char msg[128];
      std::snprintf(msg, sizeof msg, "guard: client %u reading the terrain pool is refused", c);
      probe_refuses(b, msg, false, c, slot_addr, 64);
    }
    probe_refuses(b, "guard: TERRAIN_BUILD writing one byte below the pool", true,
                  kTerrainBuildClient, kPoolBase - 64, 64);
    probe_refuses(b, "guard: TERRAIN_BUILD writing one byte past the pool", true,
                  kTerrainBuildClient, kPoolEnd - 32, 64);
    probe_admits(b, "guard: the pool's last legal 64-byte write", true, kTerrainBuildClient,
                 kPoolEnd - 64, 64);
    // The five other bank-2 regions T2 names are still unmapped, in both
    // directions. A window opened ahead of its writer is a hole with a plan.
    probe_refuses(b, "guard: TERRAIN.RESIDENT_MIP_POOL is still unmapped (write)", true,
                  kTerrainBuildClient, 0x054E0000u, 64);
    probe_refuses(b, "guard: TERRAIN.WRITEBACK_STAGING is still unmapped (write)", true,
                  kTerrainBuildClient, 0x05780000u, 64);
    probe_refuses(b, "guard: TERRAIN.WRITEBACK_STAGING is still unmapped (read)", false,
                  kTerrainBuildClient, 0x05780000u, 64);
  }

  // =========================================================================
  // 9. RANDOMIZED DIFFERENTIAL
  // -------------------------------------------------------------------------
  // THE DRAW IS FROM AN LCG'S HIGH BITS, AND THE MIX IS MEASURED. Bit k of a
  // 32-bit LCG has period 2^(k+1), so `rand % 11` visits two or three values
  // and reports full coverage; the selectors below take bits [31:24]. The
  // observed mix is then ASSERTED, because a comparison satisfied by one
  // outcome repeated has not compared anything.
  // =========================================================================
  enum Malform {
    kMalNone = 0,
    kMalUnaligned,
    kMalSlot,
    kMalEpoch,
    kMalArena,
    kMalUnreach,
    kMalWrongPatch,
    kMalGuardDeny,
    kMalBridgeErr,
    kMalNak,
    kMalDupSeq,
    kMalCount
  };
  enum AckMode { kAckPrompt = 0, kAckDelayed, kAckUnmatchedFirst, kAckModeCount };

  int mal_seen[kMalCount] = {0};
  int ackmode_seen[kAckModeCount] = {0};
  int wb_released_seen = 0, wb_withheld_seen = 0;
  int mismatches = 0;
  uint32_t lcg = 0x13579BDFu;
  auto draw = [&lcg](uint32_t n) {
    lcg = lcg * 1664525u + 1013904223u;
    return static_cast<uint32_t>((lcg >> 24) % n);
  };

  uint32_t rseq = 0x9000;
  for (int t = 0; t < 96; ++t) {
    const Malform mal = static_cast<Malform>(draw(kMalCount));
    const AckMode am = static_cast<AckMode>(draw(kAckModeCount));
    const uint32_t slot_pick = draw(2);
    const uint32_t entry = draw(kJnlEntries);
    const uint32_t island = 100 + draw(200);
    const int16_t ix = static_cast<int16_t>(static_cast<int>(draw(64)) - 32);
    const int16_t iz = static_cast<int16_t>(static_cast<int>(draw(64)) - 32);
    set_timing(b, static_cast<int>(draw(4)), static_cast<int>(draw(6)),
               static_cast<int>(draw(3)), static_cast<int>(draw(5)),
               static_cast<int>(draw(3)));

    std::vector<uint8_t> pg = make_page(island, ix, iz, 0x5000u + static_cast<uint32_t>(t) * 7u);
    if (mal == kMalWrongPatch) pg = make_page(island + 1, ix, iz, 0x777u);
    b.stage_page(slot_pick, pg);
    b.wipe_journal();

    Job j;
    j.slot = slot_pick;
    j.gen = static_cast<uint8_t>(draw(256));
    j.epoch = kEpoch;
    j.island = island;
    j.ix = ix;
    j.iz = iz;
    j.journal_addr = tp::sheet_journal_addr(entry, kJnlWindow);
    j.seq = ++rseq;
    // SOURCE IDS PROPAGATE FOR REAL: every draw gets a distinct id and every
    // completion and every trace is checked to carry it back.
    j.src_id = 0xA0000000u | static_cast<uint32_t>(t);

    uint32_t dup_seq = 0;
    if (mal == kMalSlot) j.slot = 1024 + draw(64);
    if (mal == kMalUnaligned) j.journal_addr += 8;
    if (mal == kMalEpoch) j.epoch = kEpoch + 1 + draw(100);
    if (mal == kMalArena) j.journal_addr = kJnlWindow - 64;
    if (mal == kMalUnreach) j.journal_addr |= 0x0000000400000000ull;

    // A duplicate needs a live ticket to duplicate, so one is made first.
    bool have_holder = false;
    uint32_t holder_seq = 0;
    if (mal == kMalDupSeq) {
      Job jh = j;
      jh.slot = slot_pick;
      jh.seq = ++rseq;
      jh.src_id = 0xB0000000u | static_cast<uint32_t>(t);
      Submitted sh = submit(b, jh);
      if (sh.transferred) {
        have_holder = true;
        holder_seq = jh.seq;
        oracle(jh, &pg, false, true, 1, nullptr, &want_ledger);
        dup_seq = jh.seq;
        j.seq = dup_seq;
      }
    }

    if (mal == kMalGuardDeny) {
      b.d.cfg_deny_mode_i = 1;
      b.d.cfg_deny_idx_i = static_cast<uint16_t>(draw(130));
    }
    if (mal == kMalBridgeErr) {
      b.d.cfg_err_mode_i = static_cast<uint8_t>(1 + draw(2));
      b.d.cfg_err_burst_i = static_cast<uint16_t>(draw(128));
    }
    b.d.eval();

    b.clear_stats();
    Submitted sr = submit(b, j);
    const bool stopped = (mal == kMalGuardDeny) || (mal == kMalBridgeErr);
    const bool dup = (mal == kMalDupSeq) && have_holder;

    if (mal == kMalGuardDeny) b.d.cfg_deny_mode_i = 0;
    if (mal == kMalBridgeErr) b.d.cfg_err_mode_i = 0;
    b.d.eval();

    // An unmatched ACK first, to prove it changes nothing about this job.
    if (am == kAckUnmatchedFirst) {
      const uint32_t um0 = b.d.acks_unmatched;
      send_ack(b, 0xF0000000u | static_cast<uint32_t>(t), true);
      for (int i = 0; i < 4; ++i) b.tick();
      if (b.d.acks_unmatched != um0 + 1) ++mismatches;
    }
    if (am == kAckDelayed) {
      for (int i = 0; i < 40; ++i) b.tick();
    }

    const int ack_arg = (mal == kMalNak) ? 0 : 1;
    tp::SheetWritebackResult o =
        oracle(j, &pg, dup, !stopped, ack_arg, nullptr, &want_ledger);

    if (sr.transferred) {
      send_ack(b, j.seq, mal != kMalNak);
      if (mal != kMalNak) {
        WbRec wr = take_wb(b, static_cast<int>(draw(8)));
        if (wr.timed_out || wr.slot != j.slot || wr.gen != j.gen || wr.epoch != j.epoch)
          ++mismatches;
        ++wb_released_seen;
      } else {
        if (!wb_silent(b, 12)) ++mismatches;
        ++wb_withheld_seen;
      }
    } else {
      if (!wb_silent(b, 8)) ++mismatches;
      ++wb_withheld_seen;
    }

    DoneRec dr = take_done(b, static_cast<int>(draw(5)));
    if (dr.timed_out) {
      ++mismatches;
    } else {
      if (dr.verdict != o.verdict) ++mismatches;
      if (dr.ok != o.ok) ++mismatches;
      if (dr.src_id != j.src_id) ++mismatches;
      if (dr.seq != j.seq) ++mismatches;
      if (dr.slot != (j.slot & 0x7FF)) ++mismatches;
    }
    if (o.verdict != tp::kSheetOk && b.d.fault_src_id != j.src_id) ++mismatches;

    // The payload, whenever one was supposed to land.
    if (sr.transferred && mal != kMalWrongPatch) {
      std::vector<uint8_t> want(tp::kLayerFBytes, 0);
      tp::sheet_extract(pg.data(), want.data());
      if (journal_mismatches(b, entry, want) != 0) ++mismatches;
    }

    if (have_holder) {
      send_ack(b, holder_seq, true);
      take_wb(b, 0);
      take_done(b, 0);
      ++wb_released_seen;
    }

    ++mal_seen[mal];
    ++ackmode_seen[am];
  }
  set_timing(b, 0, 0, 0, 0, 0);

  cke(0, static_cast<uint64_t>(mismatches), "random: every draw agreed with the oracle");
  {
    // THE MIX IS PRINTED, NOT JUST ASSERTED. A coverage floor that passes tells
    // you nothing about the shape above it, and the first version of this loop
    // drew two malformations only once in 44 draws -- which the floor caught
    // and a silent pass would not have.
    std::printf("random mix:");
    for (int m = 0; m < kMalCount; ++m) std::printf(" m%d=%d", m, mal_seen[m]);
    for (int m = 0; m < kAckModeCount; ++m) std::printf(" a%d=%d", m, ackmode_seen[m]);
    std::printf(" wb+%d wb-%d\n", wb_released_seen, wb_withheld_seen);
    int thin = 0;
    for (int m = 0; m < kMalCount; ++m)
      if (mal_seen[m] < 2) ++thin;
    cke(0, static_cast<uint64_t>(thin), "random: every malformation drawn at least twice");
    int thin_ack = 0;
    for (int m = 0; m < kAckModeCount; ++m)
      if (ackmode_seen[m] < 2) ++thin_ack;
    cke(0, static_cast<uint64_t>(thin_ack), "random: every ACK behaviour drawn at least twice");
    ck(wb_released_seen >= 4, "random: barriers were released", 4, wb_released_seen);
    ck(wb_withheld_seen >= 4, "random: and barriers were withheld", 4, wb_withheld_seen);
  }

  // =========================================================================
  // 10. THE WHOLE LEDGER
  // -------------------------------------------------------------------------
  // A block that answers every draw correctly while counting the wrong thing
  // passes every comparison above and fails here.
  // =========================================================================
  cke(want_ledger.sheets_written, b.d.sheets_written, "ledger: sheets_written");
  cke(want_ledger.sheets_refused, b.d.sheets_refused, "ledger: sheets_refused");
  cke(want_ledger.sheets_faulted, b.d.sheets_faulted, "ledger: sheets_faulted");
  cke(want_ledger.hdr_ident_fails, b.d.hdr_ident_fails, "ledger: hdr_ident_fails");
  cke(want_ledger.acks_ok, b.d.acks_ok, "ledger: acks_ok");
  cke(want_ledger.acks_nak, b.d.acks_nak, "ledger: acks_nak");
  cke(want_ledger.seq_conflicts, b.d.seq_conflicts, "ledger: seq_conflicts");
  // WB_BYTES COUNTS BEATS THAT RETIRED, INCLUDING AN ABORTED SHEET'S. Those
  // bytes really did land in the journal, so the counter is right to include
  // them -- and a scalar model cannot know how many there were, because
  // "the transfer stopped part way" is exactly what `transfer_complete`
  // abstracts away. So the exact check is against the BENCH's own uncleared
  // beat count, and the oracle supplies a lower bound: every COMPLETED sheet's
  // 8,192 bytes must be in there.
  cke(b.d.wbeats_total * 8, b.d.wb_bytes, "ledger: wb_bytes == the bench's own beat count x 8");
  ck(b.d.wb_bytes >= want_ledger.wb_bytes,
     "ledger: ...and it covers every completed sheet the oracle counted",
     want_ledger.wb_bytes, b.d.wb_bytes);
  ck(b.d.wb_bytes > want_ledger.wb_bytes,
     "ledger: ...with the aborted sheets' partial bytes on top", 1,
     static_cast<long long>(b.d.wb_bytes) - want_ledger.wb_bytes);
  ck(b.d.acks_unmatched > 0, "ledger: acks_unmatched moved", 1, b.d.acks_unmatched);
  ck(b.d.acks_after_epoch > 0, "ledger: acks_after_epoch moved", 1, b.d.acks_after_epoch);
  ck(b.d.acks_overdue > 0, "ledger: acks_overdue moved", 1, b.d.acks_overdue);
  ck(b.d.guard_denied > 0, "ledger: guard_denied moved", 1, b.d.guard_denied);
  ck(b.d.bridge_errs > 0, "ledger: bridge_errs moved", 1, b.d.bridge_errs);
  ck(b.d.jobs_stall_cycles > 0, "ledger: jobs_stall_cycles moved", 1, b.d.jobs_stall_cycles);
  ck(b.d.ack_wait_max_cycles > 0, "ledger: ack_wait_max_cycles moved", 1,
     b.d.ack_wait_max_cycles);
  cke(4, b.d.outstanding_hwm, "ledger: outstanding_hwm is the four-ticket high-water mark");
  ck(b.d.sheets_written >= b.d.acks_ok + b.d.acks_nak,
     "ledger: bytes-away can never lag acknowledgements", 1,
     static_cast<long long>(b.d.sheets_written) - b.d.acks_ok - b.d.acks_nak);

  std::printf("writeback_rtl_directed: %d checks, %d failures, %lld gpu clocks\n", g_checks,
              g_fail, b.cycles);
  return g_fail == 0 ? 0 : 1;
}
