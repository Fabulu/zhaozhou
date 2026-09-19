// mem_upload_directed.cpp -- MEM.UPLOAD against its oracle.
//
// `reference/include/zref/zref_mem_upload.hpp` was written BEFORE the RTL, so
// this bench compares the hardware against that oracle rather than against a
// second transcription of the same rules. The oracle owns exactly the parts
// that can be SILENTLY wrong -- whether a request may be executed at all, and
// how a {address, length} pair decomposes into bursts -- and both are "the kind
// of thing that looks right in every ordinary case and is wrong at a boundary".
//
// THE ORDER OF THE VERDICT IS PART OF THE LAW and is tested as such: a request
// that is BOTH malformed and stale must report malformed, "so a producer bug is
// never hidden behind an epoch that happened to close". A bench feeding one
// fault at a time would pass against an implementation with the priority
// backwards, so several cases carry two faults at once.
//
// EVERY REFUSAL COUNTER IS FIRED. No mutant is needed: refusing malformed
// requests is this block's JOB rather than an internal invariant, so every
// refusal is reachable from the request port with legal stimulus.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vtb_mem_upload.h"
#include "verilated.h"
#include "zhao_sim.hpp"

#include "zref/zref_mem_upload.hpp"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, long long expected, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, expected, got);
  }
}

namespace zm = zref::mem;

constexpr uint32_t kBurst = zm::kUploadBurstBytes;
constexpr uint32_t kRegionBase = 0x0400'0000;
constexpr uint32_t kRegionSize = 0x0010'0000;
constexpr uint64_t kArenaBase = 0x2000'0000ull;
constexpr uint32_t kArenaSize = 0x0010'0000;
constexpr uint16_t kEpoch = 7;

// The oracle is configured from the SAME constants the DUT is, so the two can
// never be describing different machines.
const zm::GuardRegion kRegion{kRegionBase, kRegionSize};
const zm::GuardRegion kArena{static_cast<uint32_t>(kArenaBase), kArenaSize};

void reset(Vtb_mem_upload& d) {
  d.rst_n = 0;
  d.req_valid_i = 0;
  d.req_tag_i = 0;
  d.req_hps_addr_i = 0;
  d.req_vram_addr_i = 0;
  d.req_len_i = 0;
  d.req_epoch_i = 0;
  d.req_dst_slot_i = 0;
  d.req_new_gen_i = 0;
  d.req_crc_i = 0;
  d.cfg_region_base_i = kRegionBase;
  d.cfg_region_bytes_i = kRegionSize;
  d.cfg_arena_base_i = kArenaBase;
  d.cfg_arena_bytes_i = kArenaSize;
  d.cfg_epoch_i = kEpoch;
  d.hps_req_grant_i = 0;
  d.hps_beat_valid_i = 0;
  d.hps_data_i = 0;
  d.hps_last_i = 0;
  d.hps_err_i = 0;
  d.guard_ready_i = 0;
  d.guard_ok_i = 0;
  d.guard_violation_i = 0;
  d.guard_wready_i = 1;
  d.retire_words_i = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  zhao::tick(d);
}

// WAIT FOR `req_ready_o`. The first version of this helper drove one tick and
// moved on, which silently dropped every OTHER request: the block reports a
// refusal in S_REPORT and only accepts in S_IDLE, so back-to-back requests are
// offered on cycles when it is not listening. It looked like three of six
// censuses failing to fire, i.e. like a DUT bug, and it was a bench bug.
void present(Vtb_mem_upload& d, uint64_t hps, uint32_t vram, uint32_t len, uint16_t epoch,
             uint32_t crc = 0, uint8_t slot = 3, uint16_t gen = 9, uint8_t tag = 5) {
  d.req_valid_i = 1;
  d.req_tag_i = tag;
  d.req_hps_addr_i = hps;
  d.req_vram_addr_i = vram;
  d.req_len_i = len;
  d.req_epoch_i = epoch;
  d.req_dst_slot_i = slot;
  d.req_new_gen_i = gen;
  d.req_crc_i = crc;
  for (int guard = 0; guard < 64; ++guard) {
    d.eval();
    if (d.req_ready_o) break;
    zhao::tick(d);
  }
  zhao::tick(d);
  d.req_valid_i = 0;
}

// The CRC-32C the RTL computes: zhao_crc32c_fold over every 8-byte beat,
// seeded 0xFFFFFFFF and finally inverted. Written here from that description so
// the bench is not simply echoing the DUT's own arithmetic back at it.
uint32_t crc32c_of(const std::vector<uint64_t>& beats) {
  static uint32_t table[256];
  static bool built = false;
  if (!built) {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? (0x82F63B78u ^ (c >> 1)) : (c >> 1);
      table[i] = c;
    }
    built = true;
  }
  uint32_t c = 0xFFFFFFFFu;
  for (uint64_t w : beats) {
    for (int b = 0; b < 8; ++b) {  // low byte folded FIRST
      const uint8_t byte = static_cast<uint8_t>(w >> (8 * b));
      c = table[(c ^ byte) & 0xFF] ^ (c >> 8);
    }
  }
  return ~c;
}

// ---------------------------------------------------------------------------
// 1. THE VERDICT, differentially against the oracle.
// ---------------------------------------------------------------------------
struct Case {
  const char* name;
  uint64_t hps;
  uint32_t vram;
  uint32_t len;
  uint16_t epoch;
};

const std::vector<Case>& verdict_cases() {
  static const std::vector<Case> c = {
      {"legal", kArenaBase, kRegionBase, 4 * kBurst, kEpoch},
      {"zero length", kArenaBase, kRegionBase, 0, kEpoch},
      {"unaligned length", kArenaBase, kRegionBase, kBurst + 1, kEpoch},
      {"unaligned dst", kArenaBase, kRegionBase + 1, kBurst, kEpoch},
      {"unaligned src", kArenaBase + 1, kRegionBase, kBurst, kEpoch},
      {"src above 32 bits", 0x1'0000'0000ull, kRegionBase, kBurst, kEpoch},
      {"src below arena", kArenaBase - kBurst, kRegionBase, kBurst, kEpoch},
      {"src past arena end", kArenaBase + kArenaSize, kRegionBase, kBurst, kEpoch},
      {"dst below region", kArenaBase, kRegionBase - kBurst, kBurst, kEpoch},
      {"dst past region end", kArenaBase, kRegionBase + kRegionSize, kBurst, kEpoch},
      {"dst runs off the end", kArenaBase, kRegionBase + kRegionSize - kBurst, 2 * kBurst, kEpoch},
      {"stale epoch", kArenaBase, kRegionBase, kBurst, static_cast<uint16_t>(kEpoch + 1)},
      // TWO FAULTS AT ONCE: the oracle reports the first in ITS order, and an
      // implementation with the priority backwards passes every case above.
      {"unaligned AND stale", kArenaBase, kRegionBase + 1, kBurst,
       static_cast<uint16_t>(kEpoch + 1)},
      {"zero length AND stale", kArenaBase, kRegionBase, 0, static_cast<uint16_t>(kEpoch + 1)},
      {"outside guard AND stale", kArenaBase, 0, kBurst, static_cast<uint16_t>(kEpoch + 1)},
      {"unreachable AND unaligned", 0x1'0000'0001ull, kRegionBase, kBurst, kEpoch},
      // A 32-bit sum of these WRAPS and would compare as comfortably inside.
      {"dst sum wraps 32 bits", kArenaBase, 0xFFFF'FFC0u, kBurst, kEpoch},
  };
  return c;
}

void test_verdict_matches_the_oracle() {
  for (const Case& c : verdict_cases()) {
    Vtb_mem_upload d;
    reset(d);
    const zm::UploadVerdict want =
        zm::upload_verdict(kRegion, kArena, c.hps, c.vram, c.len, c.epoch, kEpoch);
    present(d, c.hps, c.vram, c.len, c.epoch);
    d.eval();
    if (want != zm::kUploadOk) {
      check(d.done_o == 1, c.name, 1, d.done_o);
      check(d.status_o == static_cast<int>(want), c.name, static_cast<int>(want), d.status_o);
      check(d.publish_valid_o == 0, "a refused upload never publishes", 0, d.publish_valid_o);
    } else {
      check(d.done_o == 0, "a legal request starts work rather than reporting", 0, d.done_o);
      check(d.publish_valid_o == 0, "and publishes nothing yet", 0, d.publish_valid_o);
    }
  }
}

// ---------------------------------------------------------------------------
// 2. A COMPLETE upload: bursts, CRC, retire, publish.
// ---------------------------------------------------------------------------
struct Outcome {
  bool published = false;
  int status = -1;
  int generation = -1;
  int slot = -1;
  int tag = -1;
  int bursts = 0;
  bool published_before_retire = false;
  bool data_ok = true;             // every written beat was the staged beat, in order
  int reoffers_after_refusal = 0;  // requests offered AFTER the bridge refused one
};

// `refuse_burst` >= 0: the bridge REFUSES that burst's request the way the
// real `zhao_hps_bridge` does -- `err` on the request, NO grant, nothing issued.
// (`hps_error` injects an err in the middle of a granted burst, which the real
// bridge never sends; it is kept because the block must survive it anyway.)
Outcome run_upload(Vtb_mem_upload& d, uint32_t len, uint32_t crc, bool deny_guard = false,
                   bool hps_error = false, int guard_busy = 0, int refuse_burst = -1) {
  Outcome o;
  present(d, kArenaBase, kRegionBase, len, kEpoch, crc);

  const uint32_t beats_total = len / 8;
  uint32_t beats_done = 0;
  uint32_t outstanding = 0;        // the bench's own model of unretired writes
  uint32_t outstanding_words = 0;  // ... in 16-bit SDRAM words, the credit unit
  uint32_t words_written = 0;      // 64-bit beats the DUT wrote, in order
  bool verdict_pending = false;    // the guard accepted last cycle
  int reqs_accepted = 0;           // guard requests accepted so far
  int busy_left = 0;               // cycles the guard stays not-ready
  bool granted = false;            // a burst has been granted and is being served
  bool refused = false;            // the bridge has refused a request

  for (int cycle = 0; cycle < 50000; ++cycle) {
    d.eval();
    // THIS CHECK USED TO SIT BELOW THE RETURN, and so could never fire:
    // `publish_valid_o` is high only in S_REPORT, which is exactly the cycle
    // `done_o` is, and the `return` below took that cycle first. The one
    // detector this bench has for the atomicity law was structurally blind to
    // every publication. It now runs first.
    if (d.publish_valid_o && outstanding != 0) o.published_before_retire = true;
    if (d.done_o) {
      o.status = d.status_o;
      o.published = d.publish_valid_o;
      o.generation = d.publish_generation_o;
      o.slot = d.publish_slot_o;
      o.tag = d.publish_tag_o;
      return o;
    }
    // If it ever publishes while writes are still outstanding, the atomicity
    // law is broken -- that is the whole point of the RETIRE state.
    if (d.publish_valid_o && outstanding != 0) o.published_before_retire = true;

    // grant the HPS burst in the cycle it is offered -- or refuse it
    const bool issuing = d.hps_req_valid_o;
    if (issuing && refused) ++o.reoffers_after_refusal;
    const bool refuse_now = issuing && !granted && !refused && (o.bursts == refuse_burst);
    d.hps_req_grant_i = issuing && !refuse_now && !refused;
    if (refuse_now) refused = true;
    if (issuing && !granted && !refuse_now && !refused) {
      ++o.bursts;
      granted = true;
    }

    // Serve one beat per cycle only while a burst is actually in flight. The
    // first version drove beats whenever `hps_req_valid_o` was low, which
    // includes the block's own S_NEXT cycle between bursts -- so the bench
    // counted beats the DUT never took and the two desynced.
    const bool serving = granted && !issuing && (beats_done < beats_total);
    d.hps_err_i = (hps_error && serving && (beats_done == 4)) || refuse_now;
    d.hps_beat_valid_i = serving && !d.hps_err_i;
    d.hps_data_i = 0x1122334455667788ull + beats_done;
    d.hps_last_i = serving && ((beats_done % 8) == 7);

    // THE GUARD, MODELLED AS `zhao_mem_guard` BEHAVES rather than as an
    // always-ready sink. `ready` is a LEVEL (`!fwd_active`), dropped for
    // `guard_busy` cycles after every accept -- a background client's forward
    // waits behind the guaranteed clients in the arbiter -- and the verdict
    // (`ok` / `violation`) is REGISTERED: it arrives the cycle AFTER the
    // accept. The old bench raised ready and ok together, forever, so it could
    // not see a beat held against a busy guard, which is a beat lost.
    d.guard_ok_i = verdict_pending && !(deny_guard && reqs_accepted == 2);
    d.guard_violation_i = verdict_pending && deny_guard && reqs_accepted == 2;
    d.guard_ready_i = (busy_left == 0);
    d.eval();
    const bool accept_now = d.guard_req_valid_o && d.guard_ready_i;
    // RETIREMENT IN THE ARBITER'S OWN UNIT. `retire_words_i` is wired to
    // `zhao_vram_arbiter`'s `client_rsp[k].credits`, which returns 16-bit SDRAM
    // WORDS -- a 64-bit beat is FOUR of them -- one burst of up to eight words
    // at a time (`zhao_sdram_ctrl`: "rsp.credits ... returning the retired word
    // count"). This bench used to retire ONE per cycle per 64-bit beat, i.e. it
    // modelled a beat as a word; against that model a DUT that counted beats
    // looked exactly right, and against the real arbiter it published after a
    // quarter of its writes had landed. Now the bench models words and retires
    // them in the arbiter's eight-word bursts.
    // At the controller's own pace, too: `zhao_sdram_ctrl`'s write
    // grant-to-grant span is 10 cycles at best, so one 8-word credit per 10.
    // A bench that retired every cycle would drain before any early publish
    // could be seen -- the first version of this change did exactly that and
    // passed against the defect it was written to expose.
    d.retire_words_i = (outstanding_words >= 8 && (cycle % 10) == 9) ? 8 : 0;

    // COUNT WHAT THE DUT TOOK, not what the bench offered. `guard_wvalid_o` is
    // the block's own statement that this beat is being written.
    d.eval();
    const bool taken = d.guard_wvalid_o && d.guard_wready_i;
    if (taken) {
      // EVERY WRITTEN WORD IS COMPARED WITH WHAT WAS STAGED, in order. The
      // bench used to count beats and never look at them, so a lost or
      // repeated beat that still totalled correctly would have passed.
      const uint64_t want = 0x1122334455667788ull + words_written;
      if (d.guard_wdata_o != want) o.data_ok = false;
      ++words_written;
      outstanding_words += 4;
    }
    if (d.retire_words_i) outstanding_words -= d.retire_words_i;
    outstanding = outstanding_words;

    // A beat the bridge delivered is gone whether or not anyone took it.
    const bool hps_beat = d.hps_beat_valid_i;
    const bool hps_last = d.hps_last_i;

    // guard bookkeeping for the NEXT cycle
    verdict_pending = accept_now;
    if (accept_now) {
      ++reqs_accepted;
      busy_left = guard_busy;
    } else if (busy_left > 0) {
      --busy_left;
    }

    zhao::tick(d);
    if (hps_beat) ++beats_done;
    if (hps_beat && hps_last) granted = false;  // the next burst must be granted afresh
  }
  return o;
}

std::vector<uint64_t> payload_of(uint32_t len) {
  std::vector<uint64_t> v;
  for (uint32_t i = 0; i < len / 8; ++i) v.push_back(0x1122334455667788ull + i);
  return v;
}

void test_a_good_upload_publishes() {
  Vtb_mem_upload d;
  reset(d);
  const uint32_t len = 2 * kBurst;
  const uint32_t crc = crc32c_of(payload_of(len));
  const Outcome o = run_upload(d, len, crc);

  check(o.status == zm::kUploadOk, "a correct upload reports OK", zm::kUploadOk, o.status);
  check(o.published, "and PUBLISHES", 1, o.published ? 1 : 0);
  check(o.generation == 9, "publishing the new generation", 9, o.generation);
  check(o.slot == 3, "into the fresh slot it was given", 3, o.slot);
  check(o.tag == 5, "echoing the requester's tag", 5, o.tag);
  check(o.bursts == static_cast<int>(len / kBurst), "one HPS burst per 64 bytes",
        static_cast<long long>(zm::upload_bursts(len)), o.bursts);
  check(!o.published_before_retire, "and never publishes before every write retired", 0,
        o.published_before_retire ? 1 : 0);
  check(d.uploads_published_o == 1, "the published census moved", 1, d.uploads_published_o);
  check(o.data_ok, "and every written beat is the staged beat, in order", 1, o.data_ok ? 1 : 0);
}

// A BUSY GUARD. The HPS bridge's response stream has no ready, so a guard that
// is not ready when a beat arrives must never cost that beat. Before the burst
// buffer this case lost beats; 40 cycles is longer than a whole burst.
void test_a_busy_guard_loses_no_beat() {
  Vtb_mem_upload d;
  reset(d);
  const uint32_t len = 4 * kBurst;
  const uint32_t crc = crc32c_of(payload_of(len));
  const Outcome o = run_upload(d, len, crc, false, false, /*guard_busy=*/40);
  check(o.status == zm::kUploadOk, "a busy guard still ends OK", zm::kUploadOk, o.status);
  check(o.published, "and publishes", 1, o.published ? 1 : 0);
  check(o.data_ok, "with every beat written, none lost to the held guard", 1, o.data_ok ? 1 : 0);
  check(!o.published_before_retire, "and never before its writes retired", 0,
        o.published_before_retire ? 1 : 0);
}

void test_a_bad_crc_does_not_publish() {
  Vtb_mem_upload d;
  reset(d);
  const uint32_t len = 2 * kBurst;
  const uint32_t good = crc32c_of(payload_of(len));
  const Outcome o = run_upload(d, len, good ^ 1u);  // one bit wrong

  check(o.status == zm::kUploadCrcFail, "a wrong CRC is reported as such", zm::kUploadCrcFail,
        o.status);
  check(!o.published, "AND NOTHING IS PUBLISHED -- the slot is discarded unpublished", 0,
        o.published ? 1 : 0);
  check(d.refused_crc_fail_o == 1, "the CRC census fired", 1, d.refused_crc_fail_o);
  check(d.uploads_published_o == 0, "and the published census did not move", 0,
        d.uploads_published_o);
}

void test_a_denied_guard_write_does_not_publish() {
  Vtb_mem_upload d;
  reset(d);
  const uint32_t len = 4 * kBurst;
  const uint32_t crc = crc32c_of(payload_of(len));
  const Outcome o = run_upload(d, len, crc, /*deny_guard=*/true);

  // MEM.GUARD's own verdict, not a CRC failure: saying "bad checksum" about a
  // rejected address sends the next reader to the wrong question.
  check(o.status == zm::kUploadOutsideGuard, "a denied write reports the GUARD's verdict",
        zm::kUploadOutsideGuard, o.status);
  check(!o.published, "and the incomplete copy is never published", 0, o.published ? 1 : 0);
}

void test_an_hps_error_does_not_publish() {
  Vtb_mem_upload d;
  reset(d);
  const uint32_t len = 4 * kBurst;
  const uint32_t crc = crc32c_of(payload_of(len));
  const Outcome o = run_upload(d, len, crc, /*deny_guard=*/false, /*hps_error=*/true);

  check(!o.published, "a bridge error leaves the slot unpublished", 0, o.published ? 1 : 0);
  check(o.status != zm::kUploadOk, "and is not reported as success", 1,
        o.status != zm::kUploadOk ? 1 : 0);
}

// S1 (2026-09-19): the bridge refuses a request with `err` and NO grant. The
// block used to watch for `err` only in S_FILL -- after a grant the real bridge
// never follows with an err -- so in S_ISSUE it went on holding its request,
// and behind `zhao_hps_arbiter_n` that is re-served, refused again, forever.
void test_a_refused_request_is_answered_not_repeated() {
  for (int at : {0, 2}) {
    Vtb_mem_upload d;
    reset(d);
    const uint32_t len = 4 * kBurst;
    const uint32_t crc = crc32c_of(payload_of(len));
    const Outcome o = run_upload(d, len, crc, false, false, 0, /*refuse_burst=*/at);
    check(o.status >= 0, "a refused HPS request ENDS the upload (done fired)", 1,
          o.status >= 0 ? 1 : 0);
    check(o.reoffers_after_refusal == 0, "and the refused request is not offered again", 0,
          o.reoffers_after_refusal);
    check(!o.published, "and the slot is not published", 0, o.published ? 1 : 0);
    check(o.status != zm::kUploadOk, "and it is not reported as success", 1,
          o.status != zm::kUploadOk ? 1 : 0);
  }
}

// ---------------------------------------------------------------------------
// 3. EVERY REFUSAL COUNTER FIRES.
// ---------------------------------------------------------------------------
void test_every_refusal_counter_fires() {
  Vtb_mem_upload d;
  reset(d);

  present(d, kArenaBase, kRegionBase, 0, kEpoch);                // zero length
  present(d, kArenaBase, kRegionBase + 1, kBurst, kEpoch);       // unaligned
  present(d, kArenaBase, 0, kBurst, kEpoch);                     // outside guard
  present(d, kArenaBase, kRegionBase, kBurst, kEpoch + 1);       // stale epoch
  present(d, kArenaBase - kBurst, kRegionBase, kBurst, kEpoch);  // outside arena
  present(d, 0x1'0000'0000ull, kRegionBase, kBurst, kEpoch);     // unreachable
  d.eval();

  check(d.refused_zero_length_o == 1, "zero-length census fired", 1, d.refused_zero_length_o);
  check(d.refused_unaligned_o == 1, "unaligned census fired", 1, d.refused_unaligned_o);
  check(d.refused_outside_guard_o == 1, "outside-guard census fired", 1, d.refused_outside_guard_o);
  check(d.refused_epoch_stale_o == 1, "stale-epoch census fired", 1, d.refused_epoch_stale_o);
  check(d.refused_src_arena_o == 1, "source-outside-arena census fired", 1, d.refused_src_arena_o);
  check(d.refused_src_unreachable_o == 1, "source-unreachable census fired", 1,
        d.refused_src_unreachable_o);
  check(d.uploads_published_o == 0, "and nothing was published along the way", 0,
        d.uploads_published_o);
}

// ---------------------------------------------------------------------------
// 4. Burst decomposition against the oracle's own helpers.
// ---------------------------------------------------------------------------
void test_burst_decomposition_matches_the_oracle() {
  for (uint32_t bursts = 1; bursts <= 8; ++bursts) {
    Vtb_mem_upload d;
    reset(d);
    const uint32_t len = bursts * kBurst;
    const uint32_t crc = crc32c_of(payload_of(len));
    const Outcome o = run_upload(d, len, crc);
    check(o.bursts == static_cast<int>(zm::upload_bursts(len)),
          "burst count is zref::mem::upload_bursts", static_cast<long long>(zm::upload_bursts(len)),
          o.bursts);
    check(o.published, "and each legal length publishes", 1, o.published ? 1 : 0);
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  test_verdict_matches_the_oracle();
  test_a_good_upload_publishes();
  test_a_busy_guard_loses_no_beat();
  test_a_bad_crc_does_not_publish();
  test_a_denied_guard_write_does_not_publish();
  test_an_hps_error_does_not_publish();
  test_a_refused_request_is_answered_not_repeated();
  test_every_refusal_counter_fires();
  test_burst_decomposition_matches_the_oracle();

  std::printf("mem_upload_directed: %d checks, %d failed\n", g_checks, g_failed);
  std::fflush(stdout);
  // TEARDOWN-DEADLOCK WORKAROUND, documented in tests/harness/zhao_sim.hpp:
  // Verilator 5.051 + winlibs libwinpthread intermittently deadlocks in
  // VlThreadPool::~VlThreadPool() during exit-time static destruction -- the
  // process hangs with ~0 CPU in WaitForSingleObject and the verdict is lost
  // in the unflushed pipe buffer. A plain C++ return is exactly the shape
  // that hangs. I hit it on this very test and misdiagnosed it as the shell
  // blocking on a console handle; the cause was here all along and named in
  // the harness header.
  zhao::exit_hard(g_failed == 0 ? 0 : 1);
}
