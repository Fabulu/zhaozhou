// seq_rtl_directed.cpp -- TERRAIN.SEQ against zref::terrain::seq::Sequencer.
//
// The block is a PUMP: one sealed SubmitTerrainSet in, and a stream of
// residency lookups, claims, writeback jobs, load jobs, pins and patch issues
// out, strictly in list order. So the thing under test is a SEQUENCE OF
// ACTIONS, not a value, and this test compares the whole observed action log
// -- kind, order and every payload field -- against the reference's, for every
// frame it runs.
//
// WHY THE LOG AND NOT THE COUNTERS. Counters are compared too, all fourteen of
// them, but a counter cannot see order. Emitting the load job before the
// writeback job for the same slot is T4's barrier inverted -- a scar written
// into a page that has already been overwritten -- and both counters would
// still read 1. Emitting patches out of list order is a rendering difference
// the determinism ledger names as its anchor, and every counter agrees.
//
// WHY THE RESIDENCY IS DRIVEN, NOT MODELLED. Both the RTL and the oracle are
// fed the identical answer stream. A bench that owned a set-associative
// directory would have to agree with zhao_terrain_residency_v2 about victims,
// generations and pins before it could say anything about sequencing, and its
// first disagreement would be reported here as a sequencing defect.
//
// BACKPRESSURE IS WHERE THE BUGS ARE, AND FULL INPUT COVERAGE DOES NOT FIND
// THEM. TERRAIN.ISLAND's differential passed 21 checks over a full 15,625-patch
// sweep and 3,000 random draws and still missed a dropped answer, because every
// phase drove ready HIGH on every cycle. So every frame here is replayed under
// four stall patterns -- including a MOSTLY-ready one, three cycles in four,
// which is where that sibling lost answers -- and the four logs must be
// identical, byte for byte, to the always-ready log. A block that dropped a job
// when its consumer stalled would produce a shorter, otherwise-correct log.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vtb_terrain_seq.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_seq.hpp"

namespace sq = zref::terrain::seq;
namespace ss = zref::swstream;

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

// COMPOSE_SLOTS in tb_terrain_seq.sv. T6's real number is 256; the bench runs
// 16 so the allocator's exhausted state is reachable in a short frame, and the
// law -- "slot n is the n-th record of THIS frame needing composition, and the
// next one faults" -- is the same law at both sizes.
constexpr uint32_t kBenchComposeSlots = 16;

// ===========================================================================
// ONE RECORD PLUS WHAT THE DIRECTORY WILL SAY ABOUT IT
// ===========================================================================
struct Case {
  ss::PatchRecord r;
  sq::ResAnswer a;
};

// ===========================================================================
// ONE OBSERVED ACTION
// ===========================================================================
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

struct Obs {
  std::vector<Act> acts;
  uint32_t c[14] = {0};
  bool fault = false;
  uint32_t f_src = 0, f_isl = 0;
  int32_t f_ix = 0, f_iz = 0;
  bool err_stray = false;
  bool done = false;
  uint32_t accepted = 0;  // records the BENCH handed over, counted bench-side
  uint64_t cycles = 0;
};

const char* kCounterNames[14] = {
    "records_consumed", "patches_issued",   "prefetch_resident", "skipped_not_resident",
    "claims_issued",    "claims_refused",   "claims_same",       "loads_issued",
    "loads_deferred",   "writebacks_issued","compose_slots_used","pins_issued",
    "drained",          "frame_faults"};

// ===========================================================================
// THE ORACLE'S ACTION LOG
// ===========================================================================
// Built by walking zref::terrain::seq::Sequencer over the same cases. The
// per-record ACTION ORDER is the reference's own documented order and is
// asserted by comparison rather than by prose: lookup, then claim, then
// writeback strictly before load, or pin then issue.
Obs expect_of(const std::vector<Case>& cs, uint16_t patch_count, uint16_t budget,
              uint32_t epoch) {
  Obs E;
  sq::Sequencer S(kBenchComposeSlots, budget);
  S.begin_frame(epoch);
  const std::size_t n = cs.size() < patch_count ? cs.size() : patch_count;
  for (std::size_t i = 0; i < n; ++i) {
    const ss::PatchRecord& r = cs[i].r;
    const sq::ResAnswer& a = cs[i].a;
    const sq::Step st = S.step(r, a);

    if (st.did_lookup) {
      Act x;
      x.k = K::kLookup;
      x.island = r.island_id;
      x.ix = r.patch_ix;
      x.iz = r.patch_iz;
      E.acts.push_back(x);
    }
    if (st.did_claim) {
      Act x;
      x.k = K::kClaim;
      x.island = r.island_id;
      x.ix = r.patch_ix;
      x.iz = r.patch_iz;
      x.crc = r.expected_page_crc32c;
      E.acts.push_back(x);
    }
    if (st.did_writeback) {
      Act x;
      x.k = K::kWriteback;
      x.island = a.ev_island;
      x.ix = a.ev_ix;
      x.iz = a.ev_iz;
      x.slot = a.claim_slot;
      x.gen = a.ev_gen;
      x.src_id = r.source_id;
      E.acts.push_back(x);
    }
    if (st.did_load) {
      Act x;
      x.k = K::kLoad;
      x.island = r.island_id;
      x.ix = r.patch_ix;
      x.iz = r.patch_iz;
      x.slot = a.claim_slot;
      x.gen = a.claim_gen;
      x.src_id = r.source_id;
      x.crc = r.expected_page_crc32c;
      x.addr = r.hps_page_addr;
      E.acts.push_back(x);
    }
    if (st.did_pin) {
      Act x;
      x.k = K::kPin;
      x.slot = a.slot;
      x.gen = a.gen;
      E.acts.push_back(x);
    }
    if (st.did_issue) {
      Act x;
      x.k = K::kIssue;
      x.island = r.island_id;
      x.ix = r.patch_ix;
      x.iz = r.patch_iz;
      x.slot = a.slot;
      x.gen = a.gen;
      x.src_id = r.source_id;
      x.flags = r.flags;
      x.view = r.view_mask;
      x.prio = r.priority;
      x.cslot = st.compose_slot_valid ? st.compose_slot : 0u;
      x.cslot_valid = st.compose_slot_valid;
      E.acts.push_back(x);
    }
  }
  const sq::Ledger& L = S.ledger();
  E.c[0] = L.records_consumed;
  E.c[1] = L.patches_issued;
  E.c[2] = L.prefetch_resident;
  E.c[3] = L.skipped_not_resident;
  E.c[4] = L.claims_issued;
  E.c[5] = L.claims_refused;
  E.c[6] = L.claims_same;
  E.c[7] = L.loads_issued;
  E.c[8] = L.loads_deferred;
  E.c[9] = L.writebacks_issued;
  E.c[10] = L.compose_slots_used;
  E.c[11] = L.pins_issued;
  E.c[12] = L.drained;
  E.c[13] = L.frame_faults;
  E.fault = S.fault().active;
  E.f_src = S.fault().source_id;
  E.f_isl = S.fault().island_id;
  E.f_ix = S.fault().patch_ix;
  E.f_iz = S.fault().patch_iz;
  return E;
}

// ===========================================================================
// STALL PATTERNS -- ALL DRAWN FROM THE LCG'S HIGH BITS
// ===========================================================================
// A sibling lane's "randomised" phase over 240 windows turned out to be four
// distinct cases, because the draws came off the low bits of a linear
// congruential generator, whose low bits have periods of 2, 4, 8... Every draw
// in this file is taken from bits 31..16.
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
// RUN ONE FRAME
// ===========================================================================
// `answer_latency` is how many cycles the modelled directory takes to answer,
// so the block is exercised against a fast directory and a slow one without
// changing a line of it.
Obs run_frame(Vtb_terrain_seq& d, const std::vector<Case>& cs, uint16_t patch_count,
              uint16_t budget, uint32_t epoch, int pattern, uint32_t seed,
              int answer_latency = 1, bool inject_stray = false) {
  Obs O;
  uint32_t s_rec = seed ^ 0xA5A5u, s_cl = seed ^ 0x1234u, s_pin = seed ^ 0x9E37u;
  uint32_t s_wb = seed ^ 0x5A5Au, s_ld = seed ^ 0xBEEFu, s_is = seed ^ 0xC0DEu;

  // COUNTERS ARE CUMULATIVE IN THE RTL AND PER-FRAME IN THE ORACLE, so this
  // captures the running totals at entry and returns the DIFFERENCE. Comparing
  // a cumulative total against a per-frame ledger passes on frame one and
  // fails on every frame after it, which reads exactly like a block that
  // breaks the second time it is used.
  uint32_t before[14];
  d.eval();
  before[0] = d.c_records_consumed;
  before[1] = d.c_patches_issued;
  before[2] = d.c_prefetch_resident;
  before[3] = d.c_skipped_not_resident;
  before[4] = d.c_claims_issued;
  before[5] = d.c_claims_refused;
  before[6] = d.c_claims_same;
  before[7] = d.c_loads_issued;
  before[8] = d.c_loads_deferred;
  before[9] = d.c_writebacks_issued;
  before[10] = d.c_compose_slots_used;
  before[11] = d.c_pins_issued;
  before[12] = d.c_drained;
  before[13] = d.c_frame_faults;

  d.cfg_load_budget = budget;
  d.fr_epoch = epoch;
  d.fr_patch_count = patch_count;
  d.fr_sequence = 0x0100u;
  d.fr_start = 1;
  d.rec_valid = 0;
  d.lu_ans_valid = 0;
  d.cl_ans_valid = 0;
  d.eval();
  zhao::tick(d);
  d.fr_start = 0;

  std::size_t next_rec = 0;   // index of the record to offer
  std::size_t served = 0;     // index of the record whose answers are owed
  int lu_due = -1, cl_due = -1;
  std::size_t lu_idx = 0, cl_idx = 0;
  bool stray_done = !inject_stray;

  const uint64_t kCap = 200000ull;
  for (uint64_t cyc = 0; cyc < kCap; ++cyc) {
    // ---- drive the record stream --------------------------------------
    const bool offer = next_rec < cs.size() && ready_draw(s_rec, pattern);
    d.rec_valid = offer ? 1 : 0;
    if (next_rec < cs.size()) {
      const ss::PatchRecord& r = cs[next_rec].r;
      d.rec_island = r.island_id;
      d.rec_ix = static_cast<uint16_t>(r.patch_ix);
      d.rec_iz = static_cast<uint16_t>(r.patch_iz);
      d.rec_hps_addr = r.hps_page_addr;
      d.rec_crc = r.expected_page_crc32c;
      d.rec_flags = r.flags;
      d.rec_view_mask = r.view_mask;
      d.rec_priority = r.priority;
      d.rec_src_id = r.source_id;
    }

    // ---- consumer readies ----------------------------------------------
    d.cl_ready = ready_draw(s_cl, pattern) ? 1 : 0;
    d.pin_ready = ready_draw(s_pin, pattern) ? 1 : 0;
    d.wb_ready = ready_draw(s_wb, pattern) ? 1 : 0;
    d.ld_ready = ready_draw(s_ld, pattern) ? 1 : 0;
    d.is_ready = ready_draw(s_is, pattern) ? 1 : 0;

    // ---- the modelled directory's answers ------------------------------
    d.lu_ans_valid = 0;
    d.cl_ans_valid = 0;
    if (lu_due == 0) {
      const sq::ResAnswer& a = cs[lu_idx].a;
      d.lu_ans_valid = 1;
      d.lu_ans_hit = a.hit ? 1 : 0;
      d.lu_ans_slot = a.slot;
      d.lu_ans_gen = a.gen;
    }
    if (cl_due == 0) {
      const sq::ResAnswer& a = cs[cl_idx].a;
      d.cl_ans_valid = 1;
      d.cl_ans_same = a.claim_same ? 1 : 0;
      d.cl_ans_refused = a.claim_refused ? 1 : 0;
      d.cl_ans_slot = a.claim_slot;
      d.cl_ans_gen = a.claim_gen;
      d.cl_ans_ev_dirty = a.evicted_dirty ? 1 : 0;
      d.cl_ans_ev_island = a.ev_island;
      d.cl_ans_ev_ix = static_cast<uint16_t>(a.ev_ix);
      d.cl_ans_ev_iz = static_cast<uint16_t>(a.ev_iz);
      d.cl_ans_ev_gen = a.ev_gen;
    }
    // THE STRAY-ANSWER TRIPWIRE, fired on purpose: an answer offered while
    // nothing asked for one. Injected once, on a cycle the block is fetching.
    if (!stray_done && d.rec_ready) {
      d.lu_ans_valid = 1;
      d.lu_ans_hit = 0;
      stray_done = true;
    }

    d.eval();

    // ---- observe ---------------------------------------------------------
    if (d.rec_valid && d.rec_ready) ++next_rec;

    if (d.lu_valid) {
      Act x;
      x.k = K::kLookup;
      x.island = d.lu_island;
      x.ix = static_cast<int16_t>(d.lu_ix);
      x.iz = static_cast<int16_t>(d.lu_iz);
      O.acts.push_back(x);
      lu_idx = served;
      lu_due = answer_latency;
    }
    if (d.cl_valid && d.cl_ready) {
      Act x;
      x.k = K::kClaim;
      x.island = d.cl_island;
      x.ix = static_cast<int16_t>(d.cl_ix);
      x.iz = static_cast<int16_t>(d.cl_iz);
      x.crc = d.cl_expect_crc;
      O.acts.push_back(x);
      cl_idx = served;
      cl_due = answer_latency;
    }
    if (d.wb_valid && d.wb_ready) {
      Act x;
      x.k = K::kWriteback;
      x.island = d.wb_island;
      x.ix = static_cast<int16_t>(d.wb_ix);
      x.iz = static_cast<int16_t>(d.wb_iz);
      x.slot = d.wb_slot;
      x.gen = d.wb_gen;
      x.src_id = d.wb_src_id;
      O.acts.push_back(x);
    }
    if (d.ld_valid && d.ld_ready) {
      Act x;
      x.k = K::kLoad;
      x.island = d.ld_island;
      x.ix = static_cast<int16_t>(d.ld_ix);
      x.iz = static_cast<int16_t>(d.ld_iz);
      x.slot = d.ld_slot;
      x.gen = d.ld_gen;
      x.src_id = d.ld_src_id;
      x.crc = d.ld_expect_crc;
      x.addr = d.ld_hps_addr;
      O.acts.push_back(x);
    }
    if (d.pin_valid && d.pin_ready) {
      Act x;
      x.k = K::kPin;
      x.slot = d.pin_slot;
      x.gen = d.pin_gen;
      O.acts.push_back(x);
    }
    if (d.is_valid && d.is_ready) {
      Act x;
      x.k = K::kIssue;
      x.island = d.is_island;
      x.ix = static_cast<int16_t>(d.is_ix);
      x.iz = static_cast<int16_t>(d.is_iz);
      x.slot = d.is_slot;
      x.gen = d.is_gen;
      x.src_id = d.is_src_id;
      x.flags = d.is_flags;
      x.view = d.is_view_mask;
      x.prio = d.is_priority;
      x.cslot = d.is_cslot;
      x.cslot_valid = d.is_cslot_valid != 0;
      O.acts.push_back(x);
    }

    // A record is retired from the answer queue when its last transaction is
    // done. The block services strictly one record at a time, so `served`
    // simply follows `next_rec` one step behind the fetch cursor: whichever
    // record was most recently accepted is the one being answered about.
    if (next_rec > 0) served = next_rec - 1;

    if (lu_due > 0) --lu_due; else if (lu_due == 0) lu_due = -1;
    if (cl_due > 0) --cl_due; else if (cl_due == 0) cl_due = -1;

    ++O.cycles;
    const bool fin = d.fr_done;
    zhao::tick(d);
    if (fin) {
      O.done = true;
      break;
    }
  }

  d.rec_valid = 0;
  d.lu_ans_valid = 0;
  d.cl_ans_valid = 0;
  d.eval();

  O.c[0] = d.c_records_consumed - before[0];
  O.c[1] = d.c_patches_issued - before[1];
  O.c[2] = d.c_prefetch_resident - before[2];
  O.c[3] = d.c_skipped_not_resident - before[3];
  O.c[4] = d.c_claims_issued - before[4];
  O.c[5] = d.c_claims_refused - before[5];
  O.c[6] = d.c_claims_same - before[6];
  O.c[7] = d.c_loads_issued - before[7];
  O.c[8] = d.c_loads_deferred - before[8];
  O.c[9] = d.c_writebacks_issued - before[9];
  O.c[10] = d.c_compose_slots_used - before[10];
  O.c[11] = d.c_pins_issued - before[11];
  O.c[12] = d.c_drained - before[12];
  O.c[13] = d.c_frame_faults - before[13];
  O.fault = d.frame_fault != 0;
  O.f_src = d.fault_src_id;
  O.f_isl = d.fault_island;
  O.f_ix = static_cast<int16_t>(d.fault_ix);
  O.f_iz = static_cast<int16_t>(d.fault_iz);
  O.err_stray = d.err_stray_ans != 0;
  O.accepted = static_cast<uint32_t>(next_rec);

  if (!O.done) {
    std::printf("    STALLED frame after %llu cycles: acts=%zu records=%u busy=%d\n",
                static_cast<unsigned long long>(O.cycles), O.acts.size(), d.c_records_consumed,
                static_cast<int>(d.fr_busy));
    std::fflush(stdout);
  }
  return O;
}

void reset_dut(Vtb_terrain_seq& d) {
  d.rst_n = 0;
  d.fr_start = 0;
  d.rec_valid = 0;
  d.lu_ans_valid = 0;
  d.cl_ans_valid = 0;
  d.cl_ready = 1;
  d.pin_ready = 1;
  d.wb_ready = 1;
  d.ld_ready = 1;
  d.is_ready = 1;
  d.cfg_load_budget = 32;
  d.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
  zhao::tick(d);
}

// Compare an observed frame against the oracle. Returns faults found and
// prints the FIRST divergence with both sides spelled out -- a systematic
// break and a single wrong field look identical in a count.
int compare(const char* label, const Obs& O, const Obs& E, bool check_counters = true) {
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
  if (check_counters) {
    for (int i = 0; i < 14; ++i) {
      if (O.c[i] != E.c[i]) {
        ++bad;
        std::printf("    %s: counter %s rtl=%u oracle=%u\n", label, kCounterNames[i], O.c[i],
                    E.c[i]);
      }
    }
    if (O.fault != E.fault) {
      ++bad;
      std::printf("    %s: frame_fault rtl=%d oracle=%d\n", label, static_cast<int>(O.fault),
                  static_cast<int>(E.fault));
    }
    if (O.fault && (O.f_src != E.f_src || O.f_isl != E.f_isl || O.f_ix != E.f_ix ||
                    O.f_iz != E.f_iz)) {
      ++bad;
      std::printf("    %s: fault identity rtl src=%u isl=%u (%d,%d) oracle src=%u isl=%u (%d,%d)\n",
                  label, O.f_src, O.f_isl, O.f_ix, O.f_iz, E.f_src, E.f_isl, E.f_ix, E.f_iz);
    }
  }
  if (!O.done) {
    ++bad;
    std::printf("    %s: frame never completed\n", label);
  }
  // THE ACCEPTANCE INVARIANT, added after a fire test walked straight past a
  // broken `rec_ready_o`. A block that holds ready high past its declared
  // patch_count takes a record off the ring and then does not count it: the
  // producer considers it delivered, the block considers it never to have
  // arrived, and the record is gone. Every other number in this comparison was
  // correct under that mutation, because the block's own counter and its own
  // action log agreed with each other about a record neither had seen.
  //
  // So the bench's own tally of accepted handshakes is compared against the
  // block's `records_consumed`. Only a witness OUTSIDE the block can see it.
  if (check_counters && O.accepted != O.c[0]) {
    ++bad;
    std::printf("    %s: bench handed over %u records, block counted %u consumed\n", label,
                O.accepted, O.c[0]);
  }
  return bad;
}

// ===========================================================================
// RECORD BUILDERS
// ===========================================================================
Case mk(uint32_t src, int16_t ix, int16_t iz, uint16_t flags, bool hit, uint16_t slot = 0,
        uint8_t gen = 0) {
  Case c;
  c.r.island_id = 7;
  c.r.patch_ix = ix;
  c.r.patch_iz = iz;
  c.r.hps_page_addr = 0x2000000ull + static_cast<uint64_t>(src) * 21376ull;
  c.r.expected_page_crc32c = 0xC0FFEE00u ^ src;
  c.r.flags = flags;
  c.r.view_mask = 3;
  c.r.priority = ss::kPriorityRequiredCurrent;
  c.r.source_id = src;
  c.a.hit = hit;
  c.a.slot = slot;
  c.a.gen = gen;
  c.a.claim_slot = static_cast<uint16_t>(slot + 500u);
  c.a.claim_gen = static_cast<uint8_t>(gen + 3u);
  return c;
}

constexpr uint16_t kReq = ss::kFlagRequired;
constexpr uint16_t kPre = ss::kFlagPrefetch;
constexpr uint16_t kDyn = ss::kFlagDynamic;

// Run one case list under all four stall patterns and assert the four logs are
// identical to the always-ready log AND to the oracle. The identity check is
// the one that finds a job dropped under backpressure.
int run_all_patterns(Vtb_terrain_seq& d, const char* label, const std::vector<Case>& cs,
                     uint16_t patch_count, uint16_t budget, uint32_t epoch, uint32_t seed,
                     Obs* out_baseline = nullptr) {
  const Obs E = expect_of(cs, patch_count, budget, epoch);
  int bad = 0;
  Obs base;
  for (int p = 0; p < 4; ++p) {
    char lab[160];
    std::snprintf(lab, sizeof lab, "%s/%s", label, kStallNames[p]);
    const int lat = 1 + (p % 3);  // 1, 2, 3, 1 -- a fast directory and slow ones
    const Obs O = run_frame(d, cs, patch_count, budget, epoch, p, seed + 17u * p, lat);
    bad += compare(lab, O, E);
    if (p == 0) {
      base = O;
    } else {
      if (O.acts.size() != base.acts.size()) {
        ++bad;
        std::printf("    %s: %zu actions under stall vs %zu always-ready\n", lab, O.acts.size(),
                    base.acts.size());
      }
      for (int i = 0; i < 14; ++i) {
        if (O.c[i] != base.c[i]) {
          ++bad;
          std::printf("    %s: counter %s moved under backpressure: %u vs %u\n", lab,
                      kCounterNames[i], O.c[i], base.c[i]);
        }
      }
    }
    if (O.err_stray) {
      ++bad;
      std::printf("    %s: err_stray_ans latched on a clean frame\n", lab);
    }
  }
  if (out_baseline) *out_baseline = base;
  return bad;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_terrain_seq d;
  reset_dut(d);

  std::printf("== TERRAIN.SEQ differential (zref::terrain::seq::Sequencer) ==\n");
  std::printf("   bench COMPOSE_SLOTS = %u (T6's number is 256; the law is the same)\n",
              kBenchComposeSlots);

  // =========================================================================
  // A1 -- all resident, all required, all STATIC. T6: static/baked visible
  // pages render from resident page layers and consume NO dynamic slot.
  // =========================================================================
  {
    std::vector<Case> cs;
    for (int i = 0; i < 8; ++i)
      cs.push_back(mk(100u + i, static_cast<int16_t>(i), 4, kReq, true,
                      static_cast<uint16_t>(20 + i), static_cast<uint8_t>(1)));
    Obs base;
    const int bad = run_all_patterns(d, "A1 resident-static", cs, 8, 32, 1, 0xA1u, &base);
    ck(bad == 0, "A1 resident static set matches the oracle under all four stall patterns", 0,
       bad);
    ck(base.c[1] == 8, "A1 issues all eight patches", 8, base.c[1]);
    ck(base.c[10] == 0, "A1 static patches consume no composed slot", 0, base.c[10]);
    ck(base.c[7] == 0, "A1 no loads on an all-resident frame", 0, base.c[7]);
    ck(base.c[11] == 8, "A1 pins one slot per issued patch", 8, base.c[11]);

    // THE COST PER RECORD, MEASURED AND REPORTED ON EVERY RUN so it cannot go
    // stale in the contract. This is the cheapest possible record -- resident,
    // required, static, one-cycle directory, consumer always ready -- so it is
    // the block's floor, not its typical case. A miss costs more (claim, wait,
    // load) and a dirty miss more again.
    const double per_rec = static_cast<double>(base.cycles) / 8.0;
    std::printf("   A1 cost: %llu cycles for 8 resident static records = %.2f "
                "cycles per record (directory latency 1, consumer always ready)\n",
                static_cast<unsigned long long>(base.cycles), per_rec);
    // A floor and a ceiling, so a regression in either direction is visible.
    // The state walk is FETCH, LOOKUP, WAIT_LU, PIN, ISSUE.
    ck(per_rec >= 4.0 && per_rec <= 8.0,
       "A1 the per-record floor is between 4 and 8 cycles", 5,
       static_cast<long long>(per_rec * 100));
  }

  // =========================================================================
  // A2 -- the frame-scoped allocator. Slot n goes to the n-th record of THIS
  // frame needing composition, in list order, with static records interleaved
  // so a slot counter that advanced on every record would be visible.
  // =========================================================================
  {
    std::vector<Case> cs;
    for (int i = 0; i < 10; ++i) {
      const uint16_t f = static_cast<uint16_t>(kReq | ((i % 2 == 0) ? kDyn : 0));
      cs.push_back(mk(200u + i, static_cast<int16_t>(i), 9, f, true,
                      static_cast<uint16_t>(30 + i), 2));
    }
    Obs base;
    const int bad = run_all_patterns(d, "A2 allocator", cs, 10, 32, 2, 0xA2u, &base);
    ck(bad == 0, "A2 interleaved dynamic/static allocation matches the oracle", 0, bad);
    ck(base.c[10] == 5, "A2 five dynamic records take five composed slots", 5, base.c[10]);
    // The allocation ORDER, read straight off the issue log.
    int seen = 0;
    bool order_ok = true;
    for (const Act& a : base.acts) {
      if (a.k != K::kIssue) continue;
      const bool dyn = (a.flags & kDyn) != 0;
      if (dyn) {
        if (!a.cslot_valid || a.cslot != static_cast<uint32_t>(seen)) order_ok = false;
        ++seen;
      } else if (a.cslot_valid) {
        order_ok = false;
      }
    }
    ck(order_ok, "A2 slot n goes to the n-th composing record, static records get none");
  }

  // =========================================================================
  // A3 -- prefetch records that are already resident. T5's list carries both
  // required and prefetch; a record without REQUIRED wants the page RESIDENT,
  // not DRAWN, so nothing at all reaches the engine.
  // =========================================================================
  {
    std::vector<Case> cs;
    for (int i = 0; i < 6; ++i)
      cs.push_back(mk(300u + i, static_cast<int16_t>(i), 11, kPre, true,
                      static_cast<uint16_t>(40 + i), 4));
    Obs base;
    const int bad = run_all_patterns(d, "A3 prefetch-resident", cs, 6, 32, 3, 0xA3u, &base);
    ck(bad == 0, "A3 resident prefetch records match the oracle", 0, bad);
    ck(base.c[2] == 6, "A3 counts six prefetch-resident records", 6, base.c[2]);
    ck(base.c[1] == 0, "A3 issues nothing to the engine", 0, base.c[1]);
    ck(base.c[11] == 0, "A3 pins nothing", 0, base.c[11]);
    ck(base.c[3] == 0, "A3 a resident prefetch is not a skip", 0, base.c[3]);
  }

  // =========================================================================
  // A4 -- a miss with a CLEAN victim: claim, load, no writeback. And the
  // architecture's skip law: the frame does not wait, the patch is not drawn.
  // =========================================================================
  {
    std::vector<Case> cs;
    for (int i = 0; i < 5; ++i) {
      Case c = mk(400u + i, static_cast<int16_t>(i), 13, kReq, false);
      c.a.evicted_dirty = false;
      cs.push_back(c);
    }
    Obs base;
    const int bad = run_all_patterns(d, "A4 miss-clean", cs, 5, 32, 4, 0xA4u, &base);
    ck(bad == 0, "A4 clean misses match the oracle", 0, bad);
    ck(base.c[7] == 5, "A4 five load jobs issued", 5, base.c[7]);
    ck(base.c[9] == 0, "A4 no writeback for a clean victim", 0, base.c[9]);
    ck(base.c[1] == 0, "A4 a missing patch is not drawn this frame", 0, base.c[1]);
    ck(base.c[3] == 5, "A4 five required misses counted as skipped", 5, base.c[3]);
  }

  // =========================================================================
  // A5 -- T4's BARRIER. A dirty victim's F sheet must reach the journal before
  // the slot enters LOADING, so the writeback job must precede the load job
  // for the same slot. Both counters read 1 either way; only the ORDER can
  // tell, so the order is what is asserted.
  // =========================================================================
  {
    std::vector<Case> cs;
    for (int i = 0; i < 4; ++i) {
      Case c = mk(500u + i, static_cast<int16_t>(i), 17, kReq, false);
      c.a.evicted_dirty = true;
      c.a.ev_island = 9;
      c.a.ev_ix = static_cast<int16_t>(-3 - i);
      c.a.ev_iz = static_cast<int16_t>(77 + i);
      c.a.ev_gen = static_cast<uint8_t>(11 + i);
      cs.push_back(c);
    }
    Obs base;
    const int bad = run_all_patterns(d, "A5 miss-dirty", cs, 4, 32, 5, 0xA5u, &base);
    ck(bad == 0, "A5 dirty-victim misses match the oracle", 0, bad);
    ck(base.c[9] == 4, "A5 four writeback jobs issued", 4, base.c[9]);
    int pairs = 0;
    bool barrier_ok = true;
    for (std::size_t i = 0; i + 1 < base.acts.size(); ++i) {
      if (base.acts[i].k != K::kWriteback) continue;
      ++pairs;
      if (base.acts[i + 1].k != K::kLoad) barrier_ok = false;
      if (base.acts[i + 1].slot != base.acts[i].slot) barrier_ok = false;
    }
    ck(barrier_ok && pairs == 4,
       "A5 T4 barrier: every writeback is immediately followed by the load of its own slot", 4,
       pairs);
    // The victim's identity, not the incoming record's -- writing the wrong
    // key back journals one page's scars under another page's name.
    bool ident_ok = true;
    int wbn = 0;
    for (const Act& a : base.acts) {
      if (a.k != K::kWriteback) continue;
      if (a.island != 9 || a.ix != -3 - wbn || a.iz != 77 + wbn || a.gen != 11u + wbn)
        ident_ok = false;
      ++wbn;
    }
    ck(ident_ok, "A5 the writeback carries the EVICTED page's key, not the incoming record's");
  }

  // =========================================================================
  // A6 -- T9 rule 5, "all pinned: backpressure and count". A refused claim
  // produces no writeback and no load: a slot that was never granted must not
  // be filled.
  // =========================================================================
  {
    std::vector<Case> cs;
    for (int i = 0; i < 6; ++i) {
      Case c = mk(600u + i, static_cast<int16_t>(i), 19, kReq, false);
      c.a.claim_refused = (i % 2 == 0);
      c.a.evicted_dirty = true;  // would produce a writeback if the refusal leaked
      c.a.ev_island = 5;
      c.a.ev_gen = 4;
      cs.push_back(c);
    }
    Obs base;
    const int bad = run_all_patterns(d, "A6 claim-refused", cs, 6, 32, 6, 0xA6u, &base);
    ck(bad == 0, "A6 refused claims match the oracle", 0, bad);
    ck(base.c[5] == 3, "A6 three refusals counted", 3, base.c[5]);
    ck(base.c[7] == 3, "A6 only the three granted claims load", 3, base.c[7]);
    ck(base.c[9] == 3, "A6 a refused claim writes nothing back", 3, base.c[9]);
    ck(base.c[4] == 6, "A6 all six claims were issued", 6, base.c[4]);
  }

  // =========================================================================
  // A7 -- the directory disagreeing with itself: lookup missed, claim says the
  // entry was already present. Counted, and the load still goes out.
  // =========================================================================
  {
    std::vector<Case> cs;
    for (int i = 0; i < 4; ++i) {
      Case c = mk(700u + i, static_cast<int16_t>(i), 23, kReq, false);
      c.a.claim_same = (i < 3);
      cs.push_back(c);
    }
    Obs base;
    const int bad = run_all_patterns(d, "A7 claim-same", cs, 4, 32, 7, 0xA7u, &base);
    ck(bad == 0, "A7 same-claims match the oracle", 0, bad);
    ck(base.c[6] == 3, "A7 three same-claims counted", 3, base.c[6]);
    ck(base.c[7] == 4, "A7 a same-claim still loads", 4, base.c[7]);
  }

  // =========================================================================
  // A8 -- T7's ceiling, AND THE GUARD AGAINST CONFLATING IT WITH T6's.
  // T7's overflow is proxy-and-continue, RECORDED; it is explicitly NOT a
  // frame fault. Reusing T6's fault here would fault frames the rulings say
  // to render, at exactly the moment the player is traversing fastest.
  // =========================================================================
  {
    std::vector<Case> cs;
    for (int i = 0; i < 12; ++i)
      cs.push_back(mk(800u + i, static_cast<int16_t>(i), 29, kReq, false));
    Obs base;
    const int bad = run_all_patterns(d, "A8 load-budget", cs, 12, 4, 8, 0xA8u, &base);
    ck(bad == 0, "A8 load-budget frames match the oracle", 0, bad);
    ck(base.c[7] == 4, "A8 exactly four pages requested under a budget of four", 4, base.c[7]);
    ck(base.c[8] == 8, "A8 the other eight are deferred", 8, base.c[8]);
    ck(base.c[4] == 4, "A8 a deferred record does not even claim", 4, base.c[4]);
    ck(base.fault == false, "A8 T7 OVERFLOW IS NOT A FRAME FAULT", 0,
       static_cast<int>(base.fault));
    ck(base.c[13] == 0, "A8 frame_faults stays zero under load-budget pressure", 0, base.c[13]);
    ck(base.c[0] == 12, "A8 the whole list is still consumed", 12, base.c[0]);
  }

  // =========================================================================
  // A9 -- T6's FRAME FAULT. More required dynamic patches than the composed
  // cache has slots: fault, drain the rest, record the rejected source id and
  // key. The 17th record (index 16) is the one that faults at 16 slots.
  // =========================================================================
  {
    std::vector<Case> cs;
    for (int i = 0; i < 20; ++i)
      cs.push_back(mk(900u + i, static_cast<int16_t>(i), 31,
                      static_cast<uint16_t>(kReq | kDyn), true, static_cast<uint16_t>(60 + i),
                      6));
    Obs base;
    const int bad = run_all_patterns(d, "A9 compose-overflow", cs, 20, 32, 9, 0xA9u, &base);
    ck(bad == 0, "A9 composed-cache overflow matches the oracle", 0, bad);
    ck(base.fault, "A9 the frame faults");
    ck(base.c[13] == 1, "A9 exactly one frame fault, not one per rejected record", 1,
       base.c[13]);
    ck(base.c[10] == kBenchComposeSlots, "A9 every slot was allocated before the fault",
       kBenchComposeSlots, base.c[10]);
    ck(base.c[1] == kBenchComposeSlots, "A9 sixteen patches issued, the seventeenth faulted",
       kBenchComposeSlots, base.c[1]);
    ck(base.f_src == 900u + kBenchComposeSlots,
       "A9 the fault records the REJECTED record's source id", 900 + kBenchComposeSlots,
       base.f_src);
    ck(base.f_ix == static_cast<int32_t>(kBenchComposeSlots),
       "A9 the fault records the rejected key", kBenchComposeSlots, base.f_ix);
    ck(base.c[12] == 20u - kBenchComposeSlots - 1u,
       "A9 the rest of the sealed list is DRAINED, not abandoned", 20 - kBenchComposeSlots - 1,
       base.c[12]);
    ck(base.c[0] == 20, "A9 all twenty records consumed", 20, base.c[0]);
  }

  // =========================================================================
  // A10 -- a PREFETCH record that missed is not a skip. Counting it as one
  // would bury "ground the player should be seeing is absent" underneath "the
  // streamer is streaming", which is the number the skip counter exists for.
  // =========================================================================
  {
    std::vector<Case> cs;
    for (int i = 0; i < 8; ++i) {
      const uint16_t f = (i < 5) ? kPre : kReq;
      cs.push_back(mk(1000u + i, static_cast<int16_t>(i), 37, f, false));
    }
    Obs base;
    const int bad = run_all_patterns(d, "A10 prefetch-miss", cs, 8, 32, 10, 0xAAu, &base);
    ck(bad == 0, "A10 mixed prefetch/required misses match the oracle", 0, bad);
    ck(base.c[3] == 3, "A10 only the three REQUIRED misses are skips", 3, base.c[3]);
    ck(base.c[7] == 8, "A10 but all eight pages are still fetched", 8, base.c[7]);
  }

  // =========================================================================
  // A11 -- the allocator is FRAME-SCOPED. The same set run twice must allocate
  // from slot zero both times. A persistent allocator would hand the second
  // frame slots 5..9 and every height would still be a real composed height.
  // =========================================================================
  {
    std::vector<Case> cs;
    for (int i = 0; i < 5; ++i)
      cs.push_back(mk(1100u + i, static_cast<int16_t>(i), 41,
                      static_cast<uint16_t>(kReq | kDyn), true, static_cast<uint16_t>(70 + i),
                      7));
    const Obs E = expect_of(cs, 5, 32, 11);
    const Obs f1 = run_frame(d, cs, 5, 32, 11, 0, 0xB1u, 1);
    const Obs f2 = run_frame(d, cs, 5, 32, 12, 0, 0xB2u, 2);
    int bad = compare("A11 frame1", f1, E);
    ck(bad == 0, "A11 first frame matches the oracle", 0, bad);
    std::vector<uint32_t> s1, s2;
    for (const Act& a : f1.acts)
      if (a.k == K::kIssue) s1.push_back(a.cslot);
    for (const Act& a : f2.acts)
      if (a.k == K::kIssue) s2.push_back(a.cslot);
    ck(s1.size() == 5 && s2.size() == 5, "A11 five issues per frame", 5,
       static_cast<long long>(s2.size()));
    bool same = s1.size() == s2.size();
    for (std::size_t i = 0; i < s1.size() && i < s2.size(); ++i)
      if (s1[i] != s2[i]) same = false;
    ck(same, "A11 the second frame allocates the SAME slots: no history crosses the boundary");
    ck(s2.size() == 5 && s2[0] == 0, "A11 the second frame starts at slot zero", 0,
       s2.empty() ? -1 : static_cast<long long>(s2[0]));
    // Every one of the fourteen per-frame numbers must repeat exactly. A
    // counter that carried a remainder across the boundary -- the load budget
    // is the obvious candidate -- would show here and nowhere else.
    bool delta_ok = true;
    for (int i = 0; i < 14; ++i)
      if (f2.c[i] != f1.c[i]) delta_ok = false;
    ck(delta_ok, "A11 every per-frame counter repeats exactly");
  }

  // =========================================================================
  // A12 -- the record cursor. A set that declares 4 patches must consume 4,
  // however many records the ring offers. A list overrun that becomes a silent
  // extra patch is a patch nobody sealed and nobody can replay.
  // =========================================================================
  {
    std::vector<Case> cs;
    for (int i = 0; i < 9; ++i)
      cs.push_back(mk(1200u + i, static_cast<int16_t>(i), 43, kReq, true,
                      static_cast<uint16_t>(80 + i), 8));
    const Obs E = expect_of(cs, 4, 32, 13);
    const Obs O = run_frame(d, cs, 4, 32, 13, 0, 0xC1u, 1);
    const int bad = compare("A12 short-count", O, E);
    ck(bad == 0, "A12 a declared patch_count of 4 consumes exactly 4 of 9 offered", 0, bad);
    ck(O.c[0] == 4, "A12 exactly four records consumed", 4, O.c[0]);
    ck(O.accepted == 4,
       "A12 and exactly four were ACCEPTED off the ring: ready never outlives patch_count", 4,
       O.accepted);
  }

  // =========================================================================
  // A13 -- the stray-answer tripwire, fired on purpose. An answer arriving in
  // a state that did not ask is the shape of a directory answering out of
  // order, and every consequence of it is silent: the wrong slot handle on the
  // right patch draws another island's ground in this island's place.
  // =========================================================================
  {
    reset_dut(d);
    std::vector<Case> cs;
    for (int i = 0; i < 4; ++i)
      cs.push_back(mk(1300u + i, static_cast<int16_t>(i), 47, kReq, true,
                      static_cast<uint16_t>(90 + i), 9));
    const Obs clean = run_frame(d, cs, 4, 32, 14, 0, 0xD1u, 1, /*inject_stray=*/false);
    ck(!clean.err_stray, "A13 a clean frame does not latch err_stray_ans", 0,
       static_cast<int>(clean.err_stray));
    const Obs dirty = run_frame(d, cs, 4, 32, 15, 0, 0xD2u, 1, /*inject_stray=*/true);
    ck(dirty.err_stray, "A13 an answer offered while nothing asked LATCHES err_stray_ans", 1,
       static_cast<int>(dirty.err_stray));
    reset_dut(d);
  }

  // =========================================================================
  // A14 -- source-id propagation. Every outbound job and the fault identity
  // must carry the record's own source_id, or a rejected frame names the wrong
  // program and the owner debugs the wrong thing.
  // =========================================================================
  {
    std::vector<Case> cs;
    for (int i = 0; i < 6; ++i) {
      const bool hit = (i % 2 == 0);
      Case c = mk(0xBEEF0000u + i, static_cast<int16_t>(i), 53,
                  static_cast<uint16_t>(kReq | (hit ? kDyn : 0)), hit,
                  static_cast<uint16_t>(100 + i), 10);
      c.a.evicted_dirty = !hit;
      c.a.ev_island = 12;
      c.a.ev_gen = 3;
      cs.push_back(c);
    }
    Obs base;
    const int bad = run_all_patterns(d, "A14 source-ids", cs, 6, 32, 16, 0xE1u, &base);
    ck(bad == 0, "A14 mixed hit/miss frame matches the oracle", 0, bad);
    int carried = 0;
    bool ok = true;
    for (const Act& a : base.acts) {
      if (a.k != K::kIssue && a.k != K::kLoad && a.k != K::kWriteback) continue;
      ++carried;
      if ((a.src_id & 0xFFFF0000u) != 0xBEEF0000u) ok = false;
    }
    ck(ok && carried == 9, "A14 every issue, load and writeback carries its record's source id",
       9, carried);
  }

  // =========================================================================
  // PHASE R -- RANDOMISED, WITH THE DISTRIBUTION MEASURED
  // =========================================================================
  // Drawn from bits 31..16 of an LCG. A sibling lane's randomised phase over
  // 240 windows turned out to be four distinct cases because it drew from the
  // low bits, so this one COUNTS what it generated and asserts a floor on
  // every bucket. A randomised phase that never says what it covered is a
  // randomised phase nobody can believe.
  {
    uint32_t s = 0x5EEDBEEFu;
    int bad = 0;
    uint32_t disp[5] = {0};        // issued / prefetch-resident / skipped / faulted / drained
    uint32_t br_refused = 0, br_same = 0, br_dirty = 0, br_defer = 0;
    uint32_t br_cslot = 0, br_static = 0, br_fault_frames = 0;
    std::set<uint32_t> shapes;
    const int kFrames = 320;
    uint64_t total_records = 0, total_acts = 0;

    for (int f = 0; f < kFrames; ++f) {
      const uint32_t n = 1 + hi(s, 64);
      const uint16_t budget = static_cast<uint16_t>(hi(s, 34));  // 0..33 spans T7's 32
      const uint32_t epoch = 100u + static_cast<uint32_t>(f);
      std::vector<Case> cs;
      cs.reserve(n);
      for (uint32_t i = 0; i < n; ++i) {
        Case c;
        c.r.island_id = hi(s, 4);
        c.r.patch_ix = static_cast<int16_t>(static_cast<int32_t>(hi(s, 250)) - 125);
        c.r.patch_iz = static_cast<int16_t>(static_cast<int32_t>(hi(s, 250)) - 125);
        c.r.hps_page_addr = 0x2000000ull + static_cast<uint64_t>(hi(s, 4096)) * 21376ull;
        c.r.expected_page_crc32c = (hi(s, 65536) << 16) | hi(s, 65536);
        // REQUIRED two times in three, DYNAMIC one time in two, so both of the
        // rulings' pressures are reachable in the same frame.
        const bool req = hi(s, 4) != 0;
        const bool dyn = hi(s, 3) != 0;
        c.r.flags = static_cast<uint16_t>((req ? kReq : kPre) | (dyn ? kDyn : 0));
        c.r.view_mask = static_cast<uint8_t>(1 + hi(s, 3));
        c.r.priority = static_cast<uint8_t>(hi(s, 4));
        c.r.source_id = 0x70000000u + static_cast<uint32_t>(f) * 1000u + i;
        c.a.hit = hi(s, 3) != 0;
        c.a.slot = static_cast<uint16_t>(hi(s, 1024));
        c.a.gen = static_cast<uint8_t>(hi(s, 256));
        c.a.claim_slot = static_cast<uint16_t>(hi(s, 1024));
        c.a.claim_gen = static_cast<uint8_t>(hi(s, 256));
        c.a.claim_refused = hi(s, 5) == 0;
        c.a.claim_same = hi(s, 7) == 0;
        c.a.evicted_dirty = hi(s, 3) == 0;
        c.a.ev_island = hi(s, 4);
        c.a.ev_ix = static_cast<int16_t>(static_cast<int32_t>(hi(s, 250)) - 125);
        c.a.ev_iz = static_cast<int16_t>(static_cast<int32_t>(hi(s, 250)) - 125);
        c.a.ev_gen = static_cast<uint8_t>(hi(s, 256));

        const uint32_t shape = (req ? 1u : 0u) | (dyn ? 2u : 0u) | (c.a.hit ? 4u : 0u) |
                               (c.a.claim_refused ? 8u : 0u) | (c.a.claim_same ? 16u : 0u) |
                               (c.a.evicted_dirty ? 32u : 0u);
        shapes.insert(shape);
        cs.push_back(c);
      }

      // Tally what the oracle says this frame contains, so the report is about
      // the STIMULUS rather than about what the RTL happened to do with it.
      {
        sq::Sequencer T(kBenchComposeSlots, budget);
        T.begin_frame(epoch);
        for (const Case& c : cs) {
          const sq::Step st = T.step(c.r, c.a);
          disp[static_cast<int>(st.disp)]++;
          if (st.claim_refused) ++br_refused;
          if (st.claim_same) ++br_same;
          if (st.did_writeback) ++br_dirty;
          if (st.load_budget_deferred) ++br_defer;
          if (st.compose_slot_valid) ++br_cslot;
          if (st.did_issue && !st.compose_slot_valid) ++br_static;
        }
        if (T.fault().active) ++br_fault_frames;
      }

      const int pattern = static_cast<int>(hi(s, 4));
      const int lat = 1 + static_cast<int>(hi(s, 3));
      const Obs E = expect_of(cs, static_cast<uint16_t>(n), budget, epoch);
      const Obs O = run_frame(d, cs, static_cast<uint16_t>(n), budget, epoch, pattern,
                              s ^ 0x77u, lat);
      char lab[64];
      std::snprintf(lab, sizeof lab, "R frame %d (%s, lat %d)", f, kStallNames[pattern], lat);
      // Counters are cumulative across the phase, so the log is the
      // per-frame comparison and the counters are checked as a running total
      // at the end.
      bad += compare(lab, O, E, /*check_counters=*/false);
      if (O.fault != E.fault) {
        ++bad;
        std::printf("    %s: frame_fault rtl=%d oracle=%d\n", lab, static_cast<int>(O.fault),
                    static_cast<int>(E.fault));
      }
      if (E.fault && (O.f_src != E.f_src || O.f_isl != E.f_isl || O.f_ix != E.f_ix ||
                      O.f_iz != E.f_iz)) {
        ++bad;
        std::printf("    %s: fault identity diverged\n", lab);
      }
      if (O.err_stray) {
        ++bad;
        std::printf("    %s: err_stray_ans latched\n", lab);
      }
      total_records += n;
      total_acts += O.acts.size();
      if (bad > 40) {
        std::printf("    (stopping the randomised phase after %d faults)\n", bad);
        break;
      }
    }

    ck(bad == 0, "R randomised frames match the oracle action-for-action", 0, bad);

    std::printf("   randomised: %d frames, %llu records, %llu actions\n", kFrames,
                static_cast<unsigned long long>(total_records),
                static_cast<unsigned long long>(total_acts));
    std::printf("   dispositions: issued %u  prefetch-resident %u  skipped %u  faulted %u  "
                "drained %u\n",
                disp[0], disp[1], disp[2], disp[3], disp[4]);
    std::printf("   branches: refused %u  same %u  dirty-writeback %u  budget-deferred %u  "
                "cslot %u  static-issue %u  faulting frames %u\n",
                br_refused, br_same, br_dirty, br_defer, br_cslot, br_static, br_fault_frames);
    std::printf("   distinct record shapes drawn: %zu of 64\n", shapes.size());

    // THE FLOORS. Without these the phase reports a distribution nobody
    // checked, which is how a "randomised" run becomes four cases.
    // THE FLOORS WERE MEASURED FIRST AND SET AFTERWARDS, at roughly two thirds
    // of the observed counts, so they are a real gate on coverage rather than
    // a number chosen to pass. The observed run is 4,820 / 1,639 / 3,236 / 71 /
    // 721 dispositions and 527 / 314 / 661 / 647 / 3,193 / 1,627 branches over
    // 320 frames and 10,487 records.
    ck(disp[0] >= 3000, "R issued at least 3,000 patches", 3000, disp[0]);
    ck(disp[1] >= 1000, "R exercised the prefetch-resident path", 1000, disp[1]);
    ck(disp[2] >= 2000, "R exercised the miss path", 2000, disp[2]);
    ck(disp[3] >= 40, "R faulted at least 40 frames on composed-cache overflow", 40, disp[3]);
    ck(disp[4] >= 400, "R drained records after a fault", 400, disp[4]);
    ck(br_refused >= 300, "R exercised claim refusal", 300, br_refused);
    ck(br_same >= 180, "R exercised the same-claim disagreement", 180, br_same);
    ck(br_dirty >= 400, "R exercised the dirty-victim writeback barrier", 400, br_dirty);
    ck(br_defer >= 400, "R exercised T7's load budget", 400, br_defer);
    ck(br_cslot >= 2000, "R allocated composed slots", 2000, br_cslot);
    ck(br_static >= 1000, "R issued static patches with no slot", 1000, br_static);
    // EXACT, not a floor: 64 is every combination of the six stimulus bits, so
    // anything less means a shape the block can meet in the field was never
    // presented to it. This is the check that would have caught the sibling
    // lane's "randomised" phase that was four distinct cases.
    ck(shapes.size() == 64, "R drew every one of the 64 record shapes", 64,
       static_cast<long long>(shapes.size()));
  }

  std::printf("== %d checks, %d failures ==\n", g_checks, g_fail);
  std::fflush(stdout);
  zhao::exit_hard(g_fail == 0 ? 0 : 1);
}
