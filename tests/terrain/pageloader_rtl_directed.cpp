// pageloader_rtl_directed.cpp -- TERRAIN.PAGELOADER against
// zref::terrain::page_load.
//
// ---------------------------------------------------------------------------
// WHAT CAN GO SILENTLY WRONG HERE
// ---------------------------------------------------------------------------
// A page loader that works is invisible; a page loader that is subtly wrong is
// ALSO invisible, and that asymmetry decides what this file checks.
//
//  * A CRC accumulated over the wrong RANGE still produces a number. It matches
//    a staging tool that made the same mistake and mismatches one that did not,
//    and either way the failure surfaces as "terrain sometimes does not load".
//    So the two boundaries are checked by CORRUPTING JUST OUTSIDE THEM: a byte
//    in the header and a byte in the 56-byte pad must NOT change the verdict,
//    and a byte one place inside must.
//  * A page written to the wrong SLOT is a real page of real ground in the
//    wrong place. So the bench records the first and last byte address of every
//    write and compares them against `zref::terrain::page_vram_addr`.
//  * A machine that does its work TWICE produces byte-identical output. Both
//    incidents of 2026-09-05 were exactly that. So the bench counts bursts,
//    guard requests and write beats and requires 334 / 334 / 2,672 EXACTLY --
//    not "at least".
//  * A completion dropped because the directory was busy leaves a residency
//    slot in LOADING forever. So `fin_ready` is held low for a long stretch and
//    the payload is compared against itself every cycle of it.
//
// The oracle is `zref::terrain::page_load`, which composes
// `zref::mem::upload_verdict` rather than restating it -- so this test is also
// what keeps the loader's refusal order tied to the console's one upload law.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vtb_pageloader.h"

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
constexpr uint32_t kPageWords = tp::kPageBytes / 8;  // 2,672
constexpr uint32_t kHpsWindow = 0x20000000u;         // where the bench stages
constexpr uint32_t kArenaBase = 0x20000000u;
constexpr uint32_t kArenaSize = 4u * tp::kPageBytes;
constexpr uint32_t kEpoch = 0x0001BEEFu;

// The client identity. `ZHAO_CLIENT_TERRAIN_BUILD = 6` is now DECLARED in
// `zhao_pkg` (ruling T3), and MEM.GUARD gives that client -- and only that
// client -- a write window over TERRAIN.PAGE_POOL. The bench drove 6 before the
// amendment existed, on the grounds that the block forwards whatever identity
// it is given; the number has not changed, but what the machine does with it
// has, and section 7 is where that is measured.
constexpr uint32_t kTerrainBuildClient = 6;

// Every other client id the console has, so "a different client writing there"
// can be checked as a SET rather than as one example. 5 is the id ruling T3
// reserves and forbids spending -- it is in the list precisely because nothing
// declares it, and a guard that admitted an undeclared id would be admitting
// whatever a stuck bus happened to present.
constexpr uint32_t kOtherClients[] = {0, 1, 2, 3, 4, 5, 7};

// The pool's own bounds, spelled from the ruling rather than from the RTL, so
// that a wrong constant in `zhao_pkg` fails here instead of agreeing with
// itself. T2: 0x0400_0000 .. 0x054D_FFFF inclusive, 1,024 x 21,376 B.
constexpr uint32_t kPoolBase = 0x04000000u;
constexpr uint32_t kPoolEnd = 0x054E0000u;  // half-open; 0x054D_FFFF + 1

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
  wr16(&b[0], 1);           // format_version
  b[2] = 1;                 // pitch_log2 = +1, the canonical 2.0 m
  b[3] = 0;                 // flags
  wr32(&b[4], island);
  wr16(&b[8], static_cast<uint16_t>(ix));
  wr16(&b[10], static_cast<uint16_t>(iz));
  wr32(&b[12], 0xA5A50001u);  // tileset_id
  uint32_t s = seed ? seed : 1u;
  for (uint32_t i = tp::kPageHeaderBytes; i < tp::kPageBytes; ++i) {
    s = s * 1664525u + 1013904223u;
    b[i] = static_cast<uint8_t>(s >> 24);
  }
  wr32(&b[32], tp::page_payload_crc(b.data()));  // the header's own copy
  return b;
}

// ------------------------------------------------------------------- bench --
struct Bench {
  Vtb_pageloader d;
  long long cycles = 0;

  void tick() {
    zhao::tick(d);
    ++cycles;
  }

  void reset() {
    d.rst_n = 0;
    d.mw_en = 0;
    d.j_valid = 0;
    d.fin_ready = 0;
    d.stat_clear_i = 0;
    d.eval();
    for (int i = 0; i < 4; ++i) tick();
    d.rst_n = 1;
    tick();
  }

  void stage(uint32_t page_index, const std::vector<uint8_t>& page) {
    const uint32_t base_word = page_index * kPageWords;
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

  void wipe_vram() {
    d.mw_sel = 1;
    for (uint32_t w = 0; w < 4 * kPageWords; ++w) {
      d.mw_en = 1;
      d.mw_addr = static_cast<uint16_t>(w);
      d.mw_data = 0xDEADBEEFDEADBEEFull;
      tick();
    }
    d.mw_en = 0;
  }

  uint64_t vram_word(uint32_t idx) {
    d.mr_addr = static_cast<uint16_t>(idx);
    tick();
    return d.mr_data;
  }
};

struct Job {
  uint32_t slot = 0;
  uint8_t gen = 0;
  uint32_t epoch = kEpoch;
  uint32_t island = 0;
  int16_t ix = 0;
  int16_t iz = 0;
  uint64_t hps_addr = kHpsWindow;
  uint32_t expect_crc = 0;
  uint32_t src_id = 0;
};

struct Fin {
  bool ok = false;
  uint32_t crc = 0;
  int verdict = -1;
  uint32_t slot = 0;
  uint8_t gen = 0;
  uint32_t epoch = 0;
  uint32_t src_id = 0;
  bool timed_out = false;
  int held_stable = 0;  // cycles fin was held with ready low, payload unchanged
};

// One job, start to finish. `fin_hold` is how many cycles the completion is
// deliberately NOT taken -- the check that a busy directory cannot make a
// completion evaporate.
Fin run_job(Bench& b, const Job& j, int fin_hold) {
  Fin f;
  // Every bench observation below is about THIS job. A running total would let
  // a per-job assertion pass on another job's evidence -- and `first_wr_addr`
  // is a running MINIMUM, which reports slot 0 forever once slot 0 was ever
  // written.
  b.d.stat_clear_i = 1;
  b.d.eval();
  b.tick();
  b.d.stat_clear_i = 0;
  b.d.eval();

  b.d.j_slot = static_cast<uint16_t>(j.slot);
  b.d.j_gen = j.gen;
  b.d.j_epoch = j.epoch;
  b.d.j_island = j.island;
  b.d.j_ix = static_cast<uint32_t>(static_cast<int32_t>(j.ix));
  b.d.j_iz = static_cast<uint32_t>(static_cast<int32_t>(j.iz));
  b.d.j_hps_addr = j.hps_addr;
  b.d.j_expect_crc = j.expect_crc;
  b.d.j_src_id = j.src_id;
  b.d.j_valid = 1;
  b.d.eval();

  int spin = 0;
  while (!b.d.j_ready && spin++ < 64) b.tick();
  b.tick();  // the accepting edge
  b.d.j_valid = 0;
  b.d.eval();

  // wait for the completion, with a generous bound: a whole page at the sim
  // profile is ~11k cycles and the stalled cases are several times that.
  long long guard = 0;
  while (!b.d.fin_valid) {
    b.tick();
    if (++guard > 4000000LL) {
      f.timed_out = true;
      return f;
    }
  }

  // HOLD IT. Payload must not move, valid must not drop.
  const uint32_t crc0 = b.d.fin_crc;
  const uint32_t vd0 = b.d.fin_verdict;
  const uint32_t slot0 = b.d.fin_slot;
  const uint32_t src0 = b.d.fin_src_id;
  const uint32_t ok0 = b.d.fin_ok;
  bool stable = true;
  for (int i = 0; i < fin_hold; ++i) {
    b.tick();
    if (!b.d.fin_valid || b.d.fin_crc != crc0 || b.d.fin_verdict != vd0 ||
        b.d.fin_slot != slot0 || b.d.fin_src_id != src0 || b.d.fin_ok != ok0) {
      stable = false;
      break;
    }
  }
  f.held_stable = stable ? fin_hold : -1;

  f.ok = b.d.fin_ok != 0;
  f.crc = b.d.fin_crc;
  f.verdict = static_cast<int>(b.d.fin_verdict);
  f.slot = b.d.fin_slot;
  f.gen = static_cast<uint8_t>(b.d.fin_gen);
  f.epoch = b.d.fin_epoch;
  f.src_id = b.d.fin_src_id;

  b.d.fin_ready = 1;
  b.d.eval();
  b.tick();
  b.d.fin_ready = 0;
  b.d.eval();
  return f;
}

// One request put to the REAL `zhao_mem_guard` instance the bench drives
// directly, answered in COUNTER DELTAS. Deltas rather than levels because the
// verdict bits are one-cycle pulses two cycles apart from the accept -- reading
// them by hand is the exact mistake `tools/rtl/check_guard_verdict.py` exists to
// catch, and a check that samples the wrong cycle reads "no verdict" as "no
// pass" and goes green for the wrong reason.
struct ProbeVerdict {
  uint32_t ok = 0;
  uint32_t fwd = 0;   // reached the arbiter port -- what no-escape is ABOUT
  uint32_t viol = 0;
};

ProbeVerdict guard_probe(Bench& b, bool write, uint32_t client, uint32_t addr, unsigned len,
                         uint64_t be) {
  const uint32_t ok0 = b.d.p_ok_count;
  const uint32_t fwd0 = b.d.p_fwd_count;
  const uint32_t viol0 = b.d.p_viol_count;
  b.d.p_write = write ? 1 : 0;
  b.d.p_client = static_cast<uint8_t>(client);
  b.d.p_addr = addr;
  b.d.p_len = static_cast<uint8_t>(len);
  b.d.p_be = be;
  b.d.p_valid = 1;
  b.d.eval();
  int spin = 0;
  while (!b.d.p_ready && spin++ < 64) b.tick();
  b.tick();  // the accepting edge
  b.d.p_valid = 0;
  b.d.eval();
  for (int i = 0; i < 4; ++i) b.tick();  // the verdict pulse, and its forward
  ProbeVerdict v;
  v.ok = b.d.p_ok_count - ok0;
  v.fwd = b.d.p_fwd_count - fwd0;
  v.viol = b.d.p_viol_count - viol0;
  return v;
}

// The two directions, named, so a failure line says which one broke.
// THE EXPECTED VALUE IS THE ENCODING, NOT THE LITERAL 1. `ck` prints
// "expected %lld, got %lld" and the `got` here is ok*100 + fwd*10 + viol. A
// refused ADMIT is therefore got=1, and passing want=1 made the failure read
// "expected 1, got 1" -- a line that looks like a broken test rather than a
// broken guard. Found 2026-09-06 by the fire test for the terrain read arm:
// withdrawing the arm produced four of these and every one of them was
// unreadable. The encoding for an admission is 110 (ok=1, fwd=1, viol=0).
void probe_admits(Bench& b, const char* what, bool write, uint32_t client, uint32_t addr,
                  unsigned len) {
  const ProbeVerdict v = guard_probe(b, write, client, addr, len, full_be(len));
  ck(v.ok == 1 && v.fwd == 1 && v.viol == 0, what, 110,
     static_cast<long long>(v.ok * 100 + v.fwd * 10 + v.viol));
}

void probe_refuses(Bench& b, const char* what, bool write, uint32_t client, uint32_t addr,
                   unsigned len, uint64_t be) {
  const ProbeVerdict v = guard_probe(b, write, client, addr, len, be);
  ck(v.ok == 0 && v.fwd == 0 && v.viol == 1, what, 1,
     static_cast<long long>(v.ok * 100 + v.fwd * 10 + v.viol));
}

void set_timing(Bench& b, int req_latency, int beat_gap, int grant_hold, int wready_gap) {
  b.d.cfg_req_latency_i = static_cast<uint8_t>(req_latency);
  b.d.cfg_beat_gap_i = static_cast<uint8_t>(beat_gap);
  b.d.cfg_grant_hold_i = static_cast<uint8_t>(grant_hold);
  b.d.cfg_wready_gap_i = static_cast<uint8_t>(wready_gap);
}

// The oracle call for a job, given the exact bytes the bench staged.
tp::PageLoadResult oracle(const Job& j, const std::vector<uint8_t>* page, bool complete,
                          tp::PageLoadLedger* L) {
  tp::PageLoadRequest r;
  r.slot = j.slot;
  r.generation = j.gen;
  r.epoch = j.epoch;
  r.island_id = j.island;
  r.patch_ix = j.ix;
  r.patch_iz = j.iz;
  r.hps_addr = j.hps_addr;
  r.expect_crc = j.expect_crc;
  r.src_id = j.src_id;
  const zref::mem::GuardRegion arena{kArenaBase, kArenaSize};
  return tp::page_load(r, page ? page->data() : nullptr, arena, tp::kPagePoolBase,
                       tp::kPagePoolSlots, kEpoch, complete, true, true, L);
}

uint32_t g_rng = 0x13579BDFu;
uint32_t rnd() {
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 17;
  g_rng ^= g_rng << 5;
  return g_rng;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Bench b;

  // ---------------------------------------------------------------- setup --
  b.d.cfg_hps_window_base_i = kHpsWindow;
  b.d.cfg_hps_arena_base_i = kArenaBase;
  b.d.cfg_hps_arena_bytes_i = kArenaSize;
  b.d.cfg_epoch_i = kEpoch;
  b.d.cfg_vram_client_i = kTerrainBuildClient;
  b.d.cfg_hps_client_i = kTerrainBuildClient;
  b.d.cfg_region_ok_i = 1;
  b.d.cfg_err_mode_i = 0;
  b.d.cfg_err_burst_i = 0xFFFF;
  b.d.mr_addr = 0;
  b.d.p_valid = 0;
  b.d.p_write = 0;
  b.d.p_client = 0;
  b.d.p_addr = 0;
  b.d.p_len = 0;
  b.d.p_be = 0;
  // The VRAM window is the pool's slot 0; four pages fit, so slots 0..3 are all
  // observable and a page landing in the wrong one is visible rather than lost.
  b.d.cfg_vram_window_base_i = tp::kPagePoolBase;
  set_timing(b, 16, 0, 0, 0);  // the frozen sim profile, no extra stalls
  b.reset();

  cke(0, b.d.pages_loaded, "reset: pages_loaded");
  cke(0, b.d.pages_faulted, "reset: pages_faulted");
  cke(0, b.d.pages_refused, "reset: pages_refused");
  cke(0, b.d.load_bytes, "reset: load_bytes");
  ck(b.d.j_ready != 0, "reset: job port is ready");
  cke(0, b.d.fin_valid, "reset: no completion pending");

  // The shape the whole file rests on, asserted where a reader can see it.
  cke(21376, tp::kPageBytes, "page stride is 21,376 B");
  cke(334, tp::kPageBursts, "page is 334 bursts");
  cke(21256, tp::kPageCrcHi - tp::kPageCrcLo, "CRC covers 21,256 B");

  const uint32_t kIsland = 0x0000515Au;
  const int16_t kIx = 37, kIz = -19;
  std::vector<uint8_t> good = make_page(kIsland, kIx, kIz, 0xC0FFEEu);
  const uint32_t good_crc = tp::page_payload_crc(good.data());

  tp::PageLoadLedger L;

  // ------------------------------------------------------------------------
  // 1. THE GOLDEN PAGE, no stalls anywhere.
  // ------------------------------------------------------------------------
  b.wipe_vram();
  b.stage(0, good);
  Job j;
  j.slot = 0;
  j.gen = 7;
  j.island = kIsland;
  j.ix = kIx;
  j.iz = kIz;
  j.hps_addr = kHpsWindow;
  j.expect_crc = good_crc;
  j.src_id = 0x1111AAAAu;

  const uint32_t denied_g0 = b.d.guard_denied;
  const uint32_t errs_g0 = b.d.bridge_errs;
  const long long cyc0 = b.cycles;
  Fin f = run_job(b, j, 40);
  // MEASURED, not derived. The contract quotes this number, so it is printed
  // by the test that produces it rather than computed in prose.
  const long long page_cycles = b.cycles - cyc0 - 40;
  tp::PageLoadResult o = oracle(j, &good, true, &L);
  ck(!f.timed_out, "golden: completed");
  cke(o.ok ? 1 : 0, f.ok ? 1 : 0, "golden: fin_ok matches oracle");
  cke(o.verdict, f.verdict, "golden: verdict matches oracle");
  cke(o.crc_seen, f.crc, "golden: fin_crc matches oracle CRC");
  cke(j.src_id, f.src_id, "golden: source id propagates to the completion");
  cke(j.slot, f.slot, "golden: slot propagates");
  cke(j.gen, f.gen, "golden: generation propagates");
  cke(kEpoch, f.epoch, "golden: epoch propagates");
  cke(40, static_cast<uint64_t>(f.held_stable), "golden: fin held stable for 40 stalled cycles");
  cke(1, b.d.pages_loaded, "golden: pages_loaded");
  cke(tp::kPageBytes, b.d.load_bytes, "golden: load_bytes is one whole page");

  // ZERO DENIALS ON A CLEAN LOAD, and this check exists because breaking the
  // block on purpose showed the suite could not see the repo's own historical
  // guard bug. Merging S_GREQ and S_GVERD into one arm makes every PASS read as
  // a denial -- and the page still loads correctly, byte for byte, because the
  // verdict state catches it one cycle later. Every result-checking assertion
  // above stayed green. Only the count moved.
  cke(denied_g0, b.d.guard_denied, "golden: no guard denial on a clean page");
  cke(errs_g0, b.d.bridge_errs, "golden: no bridge error on a clean page");

  // THE HOW-MANY-TIMES HALF. Byte-identical output cannot see a machine that
  // did the work twice; these three numbers can.
  cke(tp::kPageBursts, b.d.bursts_seen, "golden: exactly 334 HPS bursts");
  cke(tp::kPageBursts, b.d.greqs_seen, "golden: exactly 334 guard requests");
  // THE REAL GUARD, ON THE REAL REQUEST STREAM, COUNTED. Before the amendment
  // this number was zero and that zero was the evidence. It is now 334 -- the
  // same count the played guard accepted -- and "exactly", not "at least",
  // because a machine doing its work twice produces byte-identical output.
  cke(tp::kPageBursts, b.d.shadow_ok_count, "golden: the real MEM.GUARD passed all 334");
  cke(tp::kPageBursts, b.d.shadow_fwd_count,
      "golden: and forwarded all 334 to the arbiter port");
  cke(tp::kPageBeats, b.d.wbeats_seen, "golden: exactly 2,672 write beats");
  cke(0, b.d.vram_oob, "golden: no write outside the page's own slot");
  cke(0, b.d.wlast_bad, "golden: wlast is exactly the 8th beat of every burst");

  // WHERE the bytes landed, against the reference's own address function.
  cke(tp::page_vram_addr(0), b.d.first_wr_addr, "golden: first write is at slot 0's base");
  cke(tp::page_vram_addr(0) + tp::kPageBytes - 8, b.d.last_wr_addr,
      "golden: last write is the page's final beat");

  // ...and WHAT landed: every word of the page, not a sample.
  {
    int bad = 0;
    for (uint32_t w = 0; w < kPageWords; ++w) {
      uint64_t want = 0;
      for (int k = 7; k >= 0; --k) want = (want << 8) | good[w * 8 + static_cast<uint32_t>(k)];
      if (b.vram_word(w) != want) ++bad;
    }
    cke(0, static_cast<uint64_t>(bad), "golden: all 2,672 words landed byte-exact");
  }
  // and nothing bled into the next slot
  cke(0xDEADBEEFDEADBEEFull, b.vram_word(kPageWords), "golden: slot 1 untouched");

  // ------------------------------------------------------------------------
  // 2. THE SAME PAGE WITH EVERY STALL SOURCE ENGAGED.
  // ------------------------------------------------------------------------
  b.wipe_vram();
  const uint32_t denied_s0 = b.d.guard_denied;
  set_timing(b, 16, 2, 5, 3);
  j.slot = 1;
  j.src_id = 0x2222BBBBu;
  b.d.cfg_vram_window_base_i = tp::kPagePoolBase;  // window still covers slots 0..3
  f = run_job(b, j, 25);
  o = oracle(j, &good, true, &L);
  ck(!f.timed_out, "stalled: completed");
  cke(o.verdict, f.verdict, "stalled: verdict matches oracle");
  cke(o.crc_seen, f.crc, "stalled: CRC identical to the unstalled run");
  cke(j.src_id, f.src_id, "stalled: source id propagates");
  cke(tp::kPageBursts, b.d.bursts_seen, "stalled: still exactly 334 bursts");
  cke(tp::kPageBeats, b.d.wbeats_seen, "stalled: still exactly 2,672 write beats");
  cke(0, b.d.vram_oob, "stalled: no write outside the window");
  cke(tp::page_vram_addr(1), b.d.first_wr_addr, "stalled: slot 1 addressed, not slot 0");
  {
    int bad = 0;
    for (uint32_t w = 0; w < kPageWords; ++w) {
      uint64_t want = 0;
      for (int k = 7; k >= 0; --k) want = (want << 8) | good[w * 8 + static_cast<uint32_t>(k)];
      if (b.vram_word(kPageWords + w) != want) ++bad;
    }
    cke(0, static_cast<uint64_t>(bad), "stalled: byte-identical image under stalls");
  }
  cke(2, b.d.pages_loaded, "stalled: pages_loaded is 2");
  cke(denied_s0, b.d.guard_denied, "stalled: still no guard denial");
  set_timing(b, 4, 0, 0, 0);

  // ------------------------------------------------------------------------
  // 3. THE CRC RANGE, PROVED AT BOTH EDGES.
  // ------------------------------------------------------------------------
  // Inside: byte 64 is the first covered byte.
  {
    std::vector<uint8_t> p = good;
    p[tp::kPageCrcLo] ^= 0xFF;
    wr32(&p[32], tp::page_payload_crc(p.data()));  // header agrees with itself
    b.stage(2, p);
    Job jj = j;
    jj.slot = 2;
    jj.hps_addr = kHpsWindow + 2 * tp::kPageBytes;
    jj.expect_crc = good_crc;  // ...but the JOB still expects the old page
    jj.src_id = 0x3333CCCCu;
    const uint32_t before = b.d.crc_fails;
    f = run_job(b, jj, 3);
    o = oracle(jj, &p, true, &L);
    cke(o.verdict, f.verdict, "crc lo edge: byte 64 is inside the range");
    cke(tp::kPageCrcFail, static_cast<uint64_t>(f.verdict), "crc lo edge: verdict is CRC fail");
    cke(0, f.ok ? 1 : 0, "crc lo edge: not ok");
    cke(before + 1, b.d.crc_fails, "crc lo edge: crc_fails counted");
    cke(jj.island, b.d.fault_island, "crc lo edge: fault traces the island");
    cke(static_cast<uint32_t>(static_cast<int32_t>(jj.ix)), b.d.fault_ix,
        "crc lo edge: fault traces ix");
    cke(static_cast<uint32_t>(static_cast<int32_t>(jj.iz)), b.d.fault_iz,
        "crc lo edge: fault traces iz");
    cke(jj.src_id, b.d.fault_src_id, "crc lo edge: fault traces the source id");
    cke(good_crc, b.d.fault_crc_expect, "crc lo edge: trace carries what was expected");
    ck(b.d.fault_crc_seen != good_crc, "crc lo edge: trace carries what was seen");
  }

  // Outside, below: byte 63 is header, not payload.
  {
    std::vector<uint8_t> p = good;
    p[63] ^= 0xFF;  // reserved header byte, outside [64, 21320)
    b.stage(2, p);
    Job jj = j;
    jj.slot = 2;
    jj.hps_addr = kHpsWindow + 2 * tp::kPageBytes;
    jj.expect_crc = good_crc;
    jj.src_id = 0x4444DDDDu;
    f = run_job(b, jj, 3);
    o = oracle(jj, &p, true, &L);
    cke(o.verdict, f.verdict, "crc lo edge: byte 63 is OUTSIDE the range");
    cke(tp::kPageOk, static_cast<uint64_t>(f.verdict), "crc lo edge: header byte does not fault");
    cke(good_crc, f.crc, "crc lo edge: CRC unchanged by a header byte");
  }

  // Outside, above: the 56-byte pad.
  {
    std::vector<uint8_t> p = good;
    p[tp::kPageCrcHi] ^= 0xFF;  // first pad byte
    p[tp::kPageBytes - 1] ^= 0xFF;
    b.stage(2, p);
    Job jj = j;
    jj.slot = 2;
    jj.hps_addr = kHpsWindow + 2 * tp::kPageBytes;
    jj.expect_crc = good_crc;
    jj.src_id = 0x5555EEEEu;
    f = run_job(b, jj, 3);
    o = oracle(jj, &p, true, &L);
    cke(o.verdict, f.verdict, "crc hi edge: the pad is OUTSIDE the range");
    cke(tp::kPageOk, static_cast<uint64_t>(f.verdict), "crc hi edge: pad bytes do not fault");
  }

  // Inside, at the top: byte 21,319 is the last covered byte.
  {
    std::vector<uint8_t> p = good;
    p[tp::kPageCrcHi - 1] ^= 0xFF;
    wr32(&p[32], tp::page_payload_crc(p.data()));
    b.stage(2, p);
    Job jj = j;
    jj.slot = 2;
    jj.hps_addr = kHpsWindow + 2 * tp::kPageBytes;
    jj.expect_crc = good_crc;
    jj.src_id = 0x6666FFFFu;
    f = run_job(b, jj, 3);
    o = oracle(jj, &p, true, &L);
    cke(o.verdict, f.verdict, "crc hi edge: byte 21,319 is INSIDE the range");
    cke(tp::kPageCrcFail, static_cast<uint64_t>(f.verdict), "crc hi edge: verdict is CRC fail");
  }

  // ------------------------------------------------------------------------
  // 4. THE HEADER IS A CORRUPTION CHECK, AND IT OUTRANKS THE CRC.
  // ------------------------------------------------------------------------
  {
    // A whole, valid, correctly-CRC'd page -- of the WRONG PATCH. No CRC can
    // catch this: the other patch's page has a perfectly good CRC of its own.
    std::vector<uint8_t> other = make_page(kIsland, kIx + 1, kIz, 0xC0FFEEu);
    b.stage(3, other);
    Job jj = j;
    jj.slot = 3;
    jj.hps_addr = kHpsWindow + 3 * tp::kPageBytes;
    jj.expect_crc = tp::page_payload_crc(other.data());  // the CRC is RIGHT
    jj.src_id = 0x7777AAAAu;
    const uint32_t before = b.d.hdr_ident_fails;
    f = run_job(b, jj, 3);
    o = oracle(jj, &other, true, &L);
    cke(o.verdict, f.verdict, "header ident: a valid page of the wrong patch is refused");
    cke(tp::kPageHeaderIdent, static_cast<uint64_t>(f.verdict), "header ident: verdict is 8");
    cke(before + 1, b.d.hdr_ident_fails, "header ident: counted");
    cke(0, f.ok ? 1 : 0, "header ident: not ok");
  }
  {
    // Identity wrong AND CRC wrong: identity wins, because it sends the reader
    // to the staging pointer rather than to the disk.
    std::vector<uint8_t> other = make_page(kIsland + 1, kIx, kIz, 0xC0FFEEu);
    b.stage(3, other);
    Job jj = j;
    jj.slot = 3;
    jj.hps_addr = kHpsWindow + 3 * tp::kPageBytes;
    jj.expect_crc = 0xDEADBEEFu;  // also wrong
    jj.src_id = 0x8888BBBBu;
    f = run_job(b, jj, 3);
    o = oracle(jj, &other, true, &L);
    cke(o.verdict, f.verdict, "header ident: identity outranks CRC");
    cke(tp::kPageHeaderIdent, static_cast<uint64_t>(f.verdict), "header ident: still verdict 8");
  }
  {
    // The page's OWN crc word disagrees with the payload while the job's
    // expectation is right. Two declared holders of one number; a disagreement
    // is itself a corruption.
    std::vector<uint8_t> p = good;
    wr32(&p[32], good_crc ^ 0x00010000u);
    b.stage(3, p);
    Job jj = j;
    jj.slot = 3;
    jj.hps_addr = kHpsWindow + 3 * tp::kPageBytes;
    jj.expect_crc = good_crc;
    jj.src_id = 0x9999CCCCu;
    f = run_job(b, jj, 3);
    o = oracle(jj, &p, true, &L);
    cke(o.verdict, f.verdict, "two holders: header CRC word disagreeing is a fault");
    cke(tp::kPageCrcFail, static_cast<uint64_t>(f.verdict), "two holders: verdict is CRC fail");
    cke(good_crc, f.crc, "two holders: the CRC it SAW is still reported");
  }
  {
    // format_version != 1
    std::vector<uint8_t> p = good;
    wr16(&p[0], 2);
    wr32(&p[32], tp::page_payload_crc(p.data()));
    b.stage(3, p);
    Job jj = j;
    jj.slot = 3;
    jj.hps_addr = kHpsWindow + 3 * tp::kPageBytes;
    jj.expect_crc = tp::page_payload_crc(p.data());
    jj.src_id = 0xAAAA1111u;
    f = run_job(b, jj, 3);
    o = oracle(jj, &p, true, &L);
    cke(o.verdict, f.verdict, "header ident: format_version 2 is refused");
    cke(tp::kPageHeaderIdent, static_cast<uint64_t>(f.verdict), "header ident: version verdict");
  }

  // ------------------------------------------------------------------------
  // 5. THE PRE-TRANSFER REFUSALS. Each must move ZERO bytes.
  // ------------------------------------------------------------------------
  struct Refusal {
    const char* name;
    Job j;
    int want;
  };
  std::vector<Refusal> refusals;
  {
    Job r = j;
    r.slot = 2;
    r.hps_addr = kHpsWindow + 8;  // not 64-B aligned
    r.expect_crc = good_crc;
    refusals.push_back({"unaligned hps_addr", r, tp::kPageUnaligned});
  }
  {
    Job r = j;
    r.slot = 2;
    r.hps_addr = (1ull << 32) | kHpsWindow;  // above the bridge's 32-bit port
    refusals.push_back({"hps_addr above 32 bits", r, tp::kPageSourceUnreachable});
  }
  {
    Job r = j;
    r.slot = 2;
    r.hps_addr = kArenaBase + kArenaSize;  // just past the staging arena
    refusals.push_back({"source outside the arena", r, tp::kPageSourceOutsideArena});
  }
  {
    Job r = j;
    r.slot = 2;
    r.hps_addr = kArenaBase + kArenaSize - tp::kPageBytes + 64;  // tail runs off
    refusals.push_back({"source tail past the arena", r, tp::kPageSourceOutsideArena});
  }
  {
    Job r = j;
    r.slot = tp::kPagePoolSlots;  // one past the pool
    refusals.push_back({"slot outside the page pool", r, tp::kPageOutsidePool});
  }
  {
    Job r = j;
    r.slot = 2;
    r.epoch = kEpoch + 1;
    refusals.push_back({"stale resource epoch", r, tp::kPageEpochStale});
  }
  {
    // Everything wrong at once: the ORDER decides, and it is upload_verdict's.
    Job r = j;
    r.slot = tp::kPagePoolSlots + 5;
    r.hps_addr = ((1ull << 32) | (kHpsWindow + 8));
    r.epoch = kEpoch + 99;
    refusals.push_back({"all wrong at once: slot first", r, tp::kPageOutsidePool});
  }
  {
    Job r = j;
    r.slot = 2;
    r.hps_addr = ((1ull << 32) | (kHpsWindow + 8));
    r.epoch = kEpoch + 99;
    refusals.push_back({"misaligned AND unreachable: alignment first", r, tp::kPageUnaligned});
  }

  for (const Refusal& rf : refusals) {
    const uint32_t bytes_before = b.d.load_bytes;
    const uint32_t refused_before = b.d.pages_refused;
    Job rj = rf.j;
    rj.src_id = 0xB0000000u + static_cast<uint32_t>(rf.want);
    f = run_job(b, rj, 2);
    o = oracle(rj, &good, true, &L);
    std::string n1 = std::string("refusal: ") + rf.name + " -- verdict";
    std::string n2 = std::string("refusal: ") + rf.name + " -- oracle agrees";
    std::string n3 = std::string("refusal: ") + rf.name + " -- zero bytes moved";
    std::string n4 = std::string("refusal: ") + rf.name + " -- no burst issued";
    std::string n5 = std::string("refusal: ") + rf.name + " -- counted as refused";
    std::string n6 = std::string("refusal: ") + rf.name + " -- source id traced";
    cke(static_cast<uint64_t>(rf.want), static_cast<uint64_t>(f.verdict), n1.c_str());
    cke(static_cast<uint64_t>(o.verdict), static_cast<uint64_t>(f.verdict), n2.c_str());
    cke(bytes_before, b.d.load_bytes, n3.c_str());
    cke(0, b.d.bursts_seen, n4.c_str());
    cke(refused_before + 1, b.d.pages_refused, n5.c_str());
    cke(rj.src_id, b.d.fault_src_id, n6.c_str());
    cke(0, f.ok ? 1 : 0, (std::string("refusal: ") + rf.name + " -- not ok").c_str());
  }

  // ------------------------------------------------------------------------
  // 6. THE TRANSFER STOPPING PART WAY.
  // ------------------------------------------------------------------------
  b.stage(0, good);
  {
    // MEM.GUARD refuses. Nothing more may be written and the page is faulted.
    const uint32_t denied_before = b.d.guard_denied;
    b.d.cfg_region_ok_i = 0;
    Job jj = j;
    jj.slot = 0;
    jj.hps_addr = kHpsWindow;
    jj.expect_crc = good_crc;
    jj.src_id = 0xC0DE0001u;
    f = run_job(b, jj, 5);
    o = oracle(jj, &good, false, &L);
    b.d.cfg_region_ok_i = 1;
    cke(o.verdict, f.verdict, "guard denial: verdict matches oracle");
    cke(tp::kPageIncomplete, static_cast<uint64_t>(f.verdict), "guard denial: verdict is 9");
    cke(denied_before + 1, b.d.guard_denied, "guard denial: guard_denied counted");
    cke(0, f.ok ? 1 : 0, "guard denial: never ok");
    cke(jj.src_id, b.d.fault_src_id, "guard denial: source id traced");
  }
  {
    // The bridge errs against a request, mid page.
    const uint32_t errs_before = b.d.bridge_errs;
    b.d.cfg_err_mode_i = 1;
    b.d.cfg_err_burst_i = 100;
    Job jj = j;
    jj.slot = 0;
    jj.hps_addr = kHpsWindow;
    jj.expect_crc = good_crc;
    jj.src_id = 0xC0DE0002u;
    f = run_job(b, jj, 5);
    o = oracle(jj, &good, false, &L);
    b.d.cfg_err_mode_i = 0;
    cke(o.verdict, f.verdict, "bridge err at request: verdict matches oracle");
    cke(errs_before + 1, b.d.bridge_errs, "bridge err at request: counted");
    cke(0, f.ok ? 1 : 0, "bridge err at request: never ok");
  }
  {
    // The bridge errs mid-burst, after beats have already arrived.
    const uint32_t errs_before = b.d.bridge_errs;
    const uint32_t inc_before = b.d.incomplete;
    b.d.cfg_err_mode_i = 2;
    b.d.cfg_err_burst_i = 50;
    Job jj = j;
    jj.slot = 0;
    jj.hps_addr = kHpsWindow;
    jj.expect_crc = good_crc;
    jj.src_id = 0xC0DE0003u;
    f = run_job(b, jj, 5);
    o = oracle(jj, &good, false, &L);
    b.d.cfg_err_mode_i = 0;
    cke(o.verdict, f.verdict, "bridge err mid-beat: verdict matches oracle");
    cke(errs_before + 1, b.d.bridge_errs, "bridge err mid-beat: counted");
    cke(inc_before + 1, b.d.incomplete, "bridge err mid-beat: incomplete counted");
  }

  // The block must still work afterwards -- an abort is not a wedge.
  b.wipe_vram();
  {
    Job jj = j;
    jj.slot = 0;
    jj.hps_addr = kHpsWindow;
    jj.expect_crc = good_crc;
    jj.src_id = 0xD0D00001u;
    const uint32_t loaded_before = b.d.pages_loaded;
    f = run_job(b, jj, 3);
    o = oracle(jj, &good, true, &L);
    cke(o.verdict, f.verdict, "recovery: a good page loads after three aborts");
    cke(loaded_before + 1, b.d.pages_loaded, "recovery: counted as loaded");
  }

  // ------------------------------------------------------------------------
  // 7. THE REAL MEM.GUARD: EXACTLY THESE WRITES AND NO OTHERS.
  // ------------------------------------------------------------------------
  // Not a property of this block -- a measurement of the machine it must join.
  //
  // THIS EXPECTATION IS INVERTED FROM WHAT IT WAS. Until 2026-09-06 the two
  // lines here read "the real MEM.GUARD never passed a terrain write" and "it
  // refused them, loudly", because bank 2 had no window for any client and the
  // block was unintegrable by construction. The guard amendment landed
  // (ZHAO_CLIENT_TERRAIN_BUILD = 6; TERRAIN.PAGE_POOL, that client
  // alone), so the claim being measured flipped -- and a flipped claim is worth
  // nothing without the other half, which is why the probe below asks the same
  // real block about the requests it must still refuse.
  ck(b.d.shadow_ok_seen != 0, "the real MEM.GUARD passes a terrain page write", 1,
     b.d.shadow_ok_seen);
  cke(0, b.d.shadow_violations,
      "the real MEM.GUARD refused NOTHING the loader issued, over the whole run");

  // ---- the other three directions, asked of a real guard directly ---------
  // "Passes exactly these and no others" is two claims. The observer above can
  // only ever see the first, because the loader only ever issues legal
  // requests. `u_probe_guard` is the same RTL on bench-driven wires.
  {
    const uint32_t slot_last = tp::kPagePoolSlots - 1;
    const uint32_t last_page = kPoolBase + slot_last * tp::kPageBytes;

    // --- ADMITTED: the ruled client, writing inside the pool ---------------
    probe_admits(b, "probe: TERRAIN.BUILD writes the first byte of slot 0", true,
                 kTerrainBuildClient, kPoolBase, 64);
    probe_admits(b, "probe: TERRAIN.BUILD writes the last 64 B of the pool", true,
                 kTerrainBuildClient, kPoolEnd - 64, 64);
    probe_admits(b, "probe: TERRAIN.BUILD writes the first byte of the last slot", true,
                 kTerrainBuildClient, last_page, 64);

    // --- REFUSED: a different client writing there -------------------------
    // Every other id the console has, including the unspent 5 and the NONE
    // encoding 7. One example would leave the window's ownership resting on
    // which example was picked.
    for (uint32_t c : kOtherClients) {
      char what[96];
      std::snprintf(what, sizeof(what), "probe: client %u may NOT write the page pool", c);
      probe_refuses(b, what, true, c, kPoolBase, 64, full_be(64));
    }

    // --- REFUSED: the ruled client writing OUTSIDE the pool -----------------
    // The bounds are checked at the byte, in both directions, because "as
    // narrow as the ruling allows" is a claim about exactly these two edges.
    probe_refuses(b, "probe: TERRAIN.BUILD may not write 8 B below the pool base", true,
                  kTerrainBuildClient, kPoolBase - 8, 64, full_be(64));
    probe_refuses(b, "probe: TERRAIN.BUILD may not write one byte past the pool end", true,
                  kTerrainBuildClient, kPoolEnd - 63, 64, full_be(64));
    probe_refuses(b, "probe: TERRAIN.BUILD may not write at the pool end", true,
                  kTerrainBuildClient, kPoolEnd, 64, full_be(64));
    probe_refuses(b, "probe: TERRAIN.BUILD may not write framebuffer slot 0", true,
                  kTerrainBuildClient, 0x00000000u, 64, full_be(64));
    probe_refuses(b, "probe: TERRAIN.BUILD may not write framebuffer slot 1", true,
                  kTerrainBuildClient, 0x02000000u, 64, full_be(64));
    probe_refuses(b, "probe: TERRAIN.BUILD may not write the geometry asset pool", true,
                  kTerrainBuildClient, 0x06A00000u, 64, full_be(64));
    // The five OTHER bank-2 regions ruling T2 names are deliberately NOT in the
    // map: TERRAIN.PAGE_POOL is the only one whose writer exists.
    probe_refuses(b, "probe: TERRAIN.BUILD may not write the resident mip pool", true,
                  kTerrainBuildClient, 0x054E0000u, 64, full_be(64));
    probe_refuses(b, "probe: TERRAIN.BUILD may not write the writeback journal", true,
                  kTerrainBuildClient, 0x05780000u, 64, full_be(64));

    // --- ADMITTED: the READ arm, which landed 2026-09-06 --------------------
    // THESE TWO USED TO BE `probe_refuses`, and the comment here used to read
    // "the window is write-only and that is the narrow reading, not an
    // oversight: T3 names F-sheet writeback as this client's traffic too, and
    // the block that does it does not exist yet."
    //
    // The block exists -- `fpga/rtl/terrain/zhao_terrain_writeback.sv`, whose
    // ruling is T4 -- so MEM.GUARD grew `terrain_rd_ok`, a second arm over the
    // SAME constant window with the opposite direction bit. This loader does
    // not read local SDRAM and never will; these probes are here because THIS
    // suite is where the pool's guard behaviour is enumerated at the byte, and
    // a boundary that is checked in one direction only is half a boundary.
    //
    // Kept as an ADMISSION rather than deleted: a deleted check is a check
    // nobody notices the loss of, and the pair below is what would catch the
    // arm being withdrawn again by someone who read this loader's write-only
    // traffic as the whole story.
    probe_admits(b, "probe: TERRAIN.BUILD MAY read the first byte of slot 0 (writeback's arm)",
                 false, kTerrainBuildClient, kPoolBase, 64);
    probe_admits(b, "probe: TERRAIN.BUILD MAY read the last 64 B of the pool", false,
                 kTerrainBuildClient, kPoolEnd - 64, 64);

    // ...and the read arm is bounded exactly like the write arm. The bounds are
    // shared constants in the RTL, so this is a check that they STAYED shared.
    probe_refuses(b, "probe: TERRAIN.BUILD may not read 64 B below the pool base", false,
                  kTerrainBuildClient, kPoolBase - 64, 64, full_be(64));
    probe_refuses(b, "probe: TERRAIN.BUILD may not read the 64 B straddling the pool end",
                  false, kTerrainBuildClient, kPoolEnd - 32, 64, full_be(64));
    probe_refuses(b, "probe: TERRAIN.BUILD may not read AT the pool end", false,
                  kTerrainBuildClient, kPoolEnd, 64, full_be(64));
    // The direction bit still means something everywhere it meant something
    // before: the read arm is scoped to this REGION, not granted to this client.
    probe_refuses(b, "probe: TERRAIN.BUILD may not read framebuffer slot 0", false,
                  kTerrainBuildClient, 0x00000000u, 64, full_be(64));
    probe_refuses(b, "probe: TERRAIN.BUILD may not read the geometry asset pool", false,
                  kTerrainBuildClient, 0x06A00000u, 64, full_be(64));

    // --- REFUSED: the shape law still applies inside the new window --------
    probe_refuses(b, "probe: a byte-enable hole is refused inside the pool", true,
                  kTerrainBuildClient, kPoolBase, 64, full_be(64) & ~0xFull);
    probe_refuses(b, "probe: len 0 is refused inside the pool", true, kTerrainBuildClient,
                  kPoolBase, 0, 0);
    probe_refuses(b, "probe: len 65 is refused inside the pool", true, kTerrainBuildClient,
                  kPoolBase, 65, full_be(64));
  }

  // ------------------------------------------------------------------------
  // 8. RANDOMISED DIFFERENTIAL.
  // ------------------------------------------------------------------------
  // Every draw picks its own corruption, its own malformation and its own
  // stall profile, and every one is compared against the oracle -- verdict, ok,
  // CRC and the propagated source id. The LEDGER is compared at the end, so a
  // block that gets each answer right while counting the wrong thing fails.
  set_timing(b, 3, 0, 0, 0);
  tp::PageLoadLedger RL;
  const uint32_t c_loaded0 = b.d.pages_loaded;
  const uint32_t c_faulted0 = b.d.pages_faulted;
  const uint32_t c_refused0 = b.d.pages_refused;
  const uint32_t c_crc0 = b.d.crc_fails;
  const uint32_t c_ident0 = b.d.hdr_ident_fails;
  const uint32_t c_bytes0 = b.d.load_bytes;
  const uint32_t c_denied0 = b.d.guard_denied;
  int mismatches = 0;
  int src_bad = 0;
  int saw_ok = 0, saw_crc = 0, saw_ident = 0, saw_refuse = 0;
  const int kDraws = 48;
  for (int draw = 0; draw < kDraws; ++draw) {
    const uint32_t r0 = rnd();
    const uint32_t slot = r0 % 4u;
    const uint32_t island = 0x1000u + (rnd() % 8u);
    const int16_t ix = static_cast<int16_t>(static_cast<int32_t>(rnd() % 2000u) - 1000);
    const int16_t iz = static_cast<int16_t>(static_cast<int32_t>(rnd() % 2000u) - 1000);

    std::vector<uint8_t> p = make_page(island, ix, iz, rnd() | 1u);

    Job jj;
    jj.slot = slot;
    jj.gen = static_cast<uint8_t>(rnd());
    jj.epoch = kEpoch;
    jj.island = island;
    jj.ix = ix;
    jj.iz = iz;
    jj.hps_addr = kHpsWindow + static_cast<uint64_t>(slot) * tp::kPageBytes;
    jj.expect_crc = tp::page_payload_crc(p.data());
    jj.src_id = rnd();

    switch (rnd() % 8u) {
      case 0: {  // corrupt inside the CRC range
        const uint32_t off = tp::kPageCrcLo + (rnd() % (tp::kPageCrcHi - tp::kPageCrcLo));
        p[off] ^= static_cast<uint8_t>(1u + (rnd() % 255u));
        break;
      }
      case 1: {  // corrupt outside it (header reserved bytes or pad)
        const uint32_t off =
            (rnd() % 2u) ? (36u + (rnd() % 28u)) : (tp::kPageCrcHi + (rnd() % 56u));
        p[off] ^= static_cast<uint8_t>(1u + (rnd() % 255u));
        break;
      }
      case 2:  // wrong patch in the header
        wr16(&p[8], static_cast<uint16_t>(static_cast<int16_t>(ix + 1)));
        wr32(&p[32], tp::page_payload_crc(p.data()));
        jj.expect_crc = tp::page_payload_crc(p.data());
        break;
      case 3:  // the header's own CRC word is wrong
        wr32(&p[32], tp::page_payload_crc(p.data()) ^ (1u << (rnd() % 32u)));
        break;
      case 4:
        jj.hps_addr += 8;  // unaligned
        break;
      case 5:
        jj.slot = tp::kPagePoolSlots + (rnd() % 16u);  // outside the pool
        break;
      case 6:
        jj.epoch = kEpoch + 1u + (rnd() % 100u);  // stale
        break;
      default:
        break;  // a clean load
    }

    b.stage(slot < 4 ? slot : 0, p);
    set_timing(b, static_cast<int>(rnd() % 6u), static_cast<int>(rnd() % 3u),
               static_cast<int>(rnd() % 4u), static_cast<int>(rnd() % 3u));

    f = run_job(b, jj, static_cast<int>(rnd() % 7u));
    o = oracle(jj, &p, true, &RL);

    if (f.timed_out || f.verdict != o.verdict || (f.ok ? 1 : 0) != (o.ok ? 1 : 0) ||
        (o.verdict != tp::kPageOutsidePool && o.verdict != tp::kPageUnaligned &&
         o.verdict != tp::kPageEpochStale && f.crc != o.crc_seen)) {
      ++mismatches;
      if (mismatches <= 4) {
        std::printf("  draw %d: rtl verdict %d ok %d crc %08X | ref verdict %d ok %d crc %08X\n",
                    draw, f.verdict, f.ok ? 1 : 0, f.crc, o.verdict, o.ok ? 1 : 0, o.crc_seen);
      }
    }
    if (f.src_id != jj.src_id) ++src_bad;
    if (f.held_stable < 0) ++src_bad;

    if (o.verdict == tp::kPageOk) ++saw_ok;
    else if (o.verdict == tp::kPageCrcFail) ++saw_crc;
    else if (o.verdict == tp::kPageHeaderIdent) ++saw_ident;
    else ++saw_refuse;
  }

  cke(0, static_cast<uint64_t>(mismatches), "random: every draw matches the oracle");
  cke(0, static_cast<uint64_t>(src_bad), "random: source id and completion held on every draw");
  // A run that only ever saw one outcome would satisfy the comparison above and
  // prove nothing, so the mix is asserted too.
  ck(saw_ok > 0, "random: the run contained clean loads", 1, saw_ok);
  ck(saw_crc > 0, "random: the run contained CRC failures", 1, saw_crc);
  ck(saw_ident > 0, "random: the run contained identity failures", 1, saw_ident);
  ck(saw_refuse > 0, "random: the run contained pre-transfer refusals", 1, saw_refuse);

  // THE LEDGER, not a spot check. A block that answers every draw correctly
  // while counting the wrong thing passes everything above and fails here.
  cke(RL.pages_loaded, b.d.pages_loaded - c_loaded0, "random: pages_loaded ledger");
  cke(RL.pages_faulted, b.d.pages_faulted - c_faulted0, "random: pages_faulted ledger");
  cke(RL.pages_refused, b.d.pages_refused - c_refused0, "random: pages_refused ledger");
  cke(RL.crc_fails, b.d.crc_fails - c_crc0, "random: crc_fails ledger");
  cke(RL.hdr_ident_fails, b.d.hdr_ident_fails - c_ident0, "random: hdr_ident_fails ledger");
  cke(RL.load_bytes, b.d.load_bytes - c_bytes0, "random: load_bytes ledger");
  cke(c_denied0, b.d.guard_denied, "random: 48 draws, not one guard denial");

  std::printf("one page at the sim profile: %lld gpu clocks\n", page_cycles);
  std::printf("random mix: ok %d, crc %d, ident %d, refused %d over %d draws\n", saw_ok, saw_crc,
              saw_ident, saw_refuse, kDraws);
  std::printf("bench totals: bursts %u, guard requests %u, write beats %u, cycles %lld\n",
              b.d.bursts_seen, b.d.greqs_seen, b.d.wbeats_seen, b.cycles);
  std::printf("real guard: passed %u, forwarded %u, refused %u | probe: ok %u, fwd %u, viol %u\n",
              b.d.shadow_ok_count, b.d.shadow_fwd_count, b.d.shadow_violations, b.d.p_ok_count,
              b.d.p_fwd_count, b.d.p_viol_count);

  std::printf("checks %d, failures %d\n", g_checks, g_fail);
  zhao::exit_hard(g_fail == 0 ? 0 : 1);
}
