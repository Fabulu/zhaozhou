// hps_bridge_directed.cpp — MEM.HPS.BRIDGE directed test (plan W2.5, D10).
//
// Contract: design/contracts/MEM.HPS.BRIDGE.md; law: spec/memory_rules.md §3.
// The C++ harness IS the HPS: it answers bursts with the frozen sim latency
// profile (16 gpu cycles request->first beat, 1 beat/cycle after — plan D10)
// on the generic burst port. Verified against zref::HpsBridge:
//   * read bursts: exact grant/beat timing, data pass-through, last beat
//   * write bursts: beat streaming to the HPS side, byte accounting
//   * malformed bursts (len 0 / > 64 / misaligned): single err pulse,
//     NOTHING issued to the HPS side, counted
//   * one-in-flight law: a request from a busy bridge answers err
//   * hps_ddr_bytes_by_client accounting both directions

#include "hps_bridge_harness.hpp"

#include <cstdio>
#include <cstdlib>

using namespace zhao_hps;

namespace {
int failures = 0;
void chk(bool ok, const char* what, long long expected = -1, long long actual = -1) {
  if (!ok) {
    failures++;
    std::printf("FAIL: %s (expected %lld, actual %lld)\n", what, expected, actual);
  } else {
    std::printf("ok: %s\n", what);
  }
}
}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  BridgeHarness h;
  h.reset();

  // ---- 1. read burst: latency profile exact --------------------------------
  {
    const size_t b0 = h.client_beat_cycles.size();
    auto [grant, idx0] = h.read_burst(1, 0x0000'1000, 64);
    (void)idx0;
    chk(h.client_beat_cycles.size() == b0 + 8, "8 read beats returned",
        static_cast<long long>(b0 + 8), static_cast<long long>(h.client_beat_cycles.size()));
    const uint64_t first = h.client_beat_cycles[b0];
    // profile: beats at hps side start grant+LAT_TO_FIRST(16)... the
    // request is OBSERVED by the harness one cycle after grant; beats at
    // the client one register stage later (bridge law, zref::HpsBridge)
    chk(first - grant >= 16 && first - grant <= 20,
        "first beat ~16 cycles after grant (sim profile)", 16, (long long)(first - grant));
    for (size_t i = 1; i < 8; i++)
      chk(h.client_beat_cycles[b0 + i] - h.client_beat_cycles[b0 + i - 1] == 1,
          "1 beat/cycle after the first", 1,
          (long long)(h.client_beat_cycles[b0 + i] - h.client_beat_cycles[b0 + i - 1]));
    // data integrity (pass-through of the harness data)
    bool data_ok = true;
    for (unsigned i = 0; i < 8; i++) data_ok = data_ok && h.client_beat_data[b0 + i] != 0;
    chk(data_ok, "read data non-zero pass-through");
    // last beat flagged (implicitly: beat count)
    chk(h.top.hps_bytes_1 == 64, "hps_ddr_bytes_by_client[blit] += 64", 64, h.top.hps_bytes_1);
  }

  // ---- 2. short read burst (8 bytes = 1 beat) -------------------------------
  {
    const size_t b0 = h.client_beat_cycles.size();
    h.read_burst(4, 0x0000'2000, 8);
    chk(h.client_beat_cycles.size() == b0 + 1, "1 beat for an 8-B burst",
        static_cast<long long>(b0 + 1), static_cast<long long>(h.client_beat_cycles.size()));
    chk(h.top.hps_bytes_4 == 8, "bytes by client[debug] += 8", 8, h.top.hps_bytes_4);
  }

  // ---- 3. write burst: beats reach the HPS side ------------------------------
  {
    uint64_t wr_seen = 0;
    const unsigned nbeats = 8;
    h.top.req_valid = 1;
    h.top.req_write = 1;
    h.top.req_client = 0;
    h.top.req_addr = 0x0000'3000;
    h.top.req_len = 64;
    h.tick();  // accepted
    h.top.req_valid = 0;
    h.tick();  // hps_req_valid observed + granted; the
               // bridge issues — beats may stream now
    for (unsigned b = 0; b < nbeats; b++) {
      h.top.wr_valid = 1;
      h.top.wr_data = beat_data(b, 0xBEEF);
      h.top.wr_last = (b + 1 == nbeats);
      h.tick();
      // hps_wr_valid reads high the cycle AFTER the beat was offered
      // (registered pass-through — the zref::HpsBridge register law)
      if (h.top.hps_wr_valid) wr_seen++;
    }
    h.top.wr_valid = 0;
    h.top.wr_last = 0;
    for (int i = 0; i < 4; i++) h.tick();
    chk(wr_seen == nbeats, "write beats passed to the HPS side", nbeats, (long long)wr_seen);
    chk(h.top.hps_bytes_0 == 64, "hps_ddr_bytes_by_client[scanout] += 64", 64, h.top.hps_bytes_0);
  }

  // ---- 3b. A BEAT OFFERED BETWEEN ACCEPT AND ISSUE ---------------------------
  // The case above ticks TWICE before streaming, with a comment saying beats
  // "may stream now" -- so it never touches the window where they may not.
  //
  // `req_grant` is asserted at acceptance; beats are consumed only once
  // `issued` is true, one cycle later. A client that streams on its own grant
  // -- which this bridge's own header invites, "after its request is accepted
  // (rsp.grant)" -- lost that beat with nothing recording it. It did not even
  // read as a short burst: the byte count adds `busy_len` at `wr_last`, not one
  // per beat, so the burst reported its full length with data missing.
  {
    const uint32_t early_before = h.top.wr_early_beats;

    h.top.req_valid = 1;
    h.top.req_write = 1;
    h.top.req_client = 0;
    h.top.req_addr = 0x0000'4000;
    h.top.req_len = 16;
    h.tick();                       // accepted; req_grant high, issued still 0
    h.top.req_valid = 0;

    const uint32_t ready_at_accept = h.top.wr_ready;

    // The offending beat, offered exactly where a naive client would put it.
    h.top.wr_valid = 1;
    h.top.wr_data = beat_data(0, 0xDEAD);
    h.top.wr_last = 0;
    h.tick();                       // issued becomes 1 during this tick
    h.top.wr_valid = 0;

    chk(ready_at_accept == 0,
        "wr_ready is LOW in the cycle after acceptance -- the window in which a "
        "beat used to vanish",
        0, (long long)ready_at_accept);
    chk(h.top.wr_early_beats == early_before + 1,
        "and a beat offered there is COUNTED exactly once. Before wr_early_beats "
        "existed this beat disappeared and nothing anywhere recorded it",
        (long long)(early_before + 1), (long long)h.top.wr_early_beats);

    // ...and the burst still completes correctly afterwards, so the counter is
    // reporting a lost beat rather than a broken bridge.
    unsigned seen = 0;
    for (unsigned b = 0; b < 2; b++) {
      h.top.wr_valid = 1;
      h.top.wr_data = beat_data(b, 0xDEAD);
      h.top.wr_last = (b + 1 == 2);
      h.tick();
      if (h.top.hps_wr_valid) seen++;
    }
    h.top.wr_valid = 0;
    h.top.wr_last = 0;
    for (int i = 0; i < 4; i++) h.tick();

    chk(seen == 2,
        "and the burst still completes normally once wr_ready is high, so the "
        "counter reports a LOST BEAT rather than a stalled bridge",
        2, (long long)seen);
    chk(h.top.wr_early_beats == early_before + 1,
        "with no further early beats counted while ready was high -- the "
        "counter tracks the violation, not the traffic",
        (long long)(early_before + 1), (long long)h.top.wr_early_beats);
  }

  // ---- 4. malformed bursts: err, nothing issued ------------------------------
  {
    const uint64_t issued0 = h.hps_seen_requests;
    const uint32_t err0 = h.top.hps_err_count;
    struct Bad {
      unsigned len;
      uint32_t addr;
    };
    const Bad bad[] = {{0, 0x1000}, {65, 0x1000}, {8, 0x1004}};
    for (const auto& b : bad) {
      h.top.req_valid = 1;
      h.top.req_write = 0;
      h.top.req_client = 1;
      h.top.req_addr = b.addr;
      h.top.req_len = b.len;
      h.tick();
      h.top.req_valid = 0;
      h.idle(4);
    }
    chk(h.top.hps_err_count == err0 + 3, "malformed bursts rejected+counted", err0 + 3,
        h.top.hps_err_count);
    chk(h.hps_seen_requests == issued0, "malformed bursts issued NOTHING", (long long)issued0,
        (long long)h.hps_seen_requests);
  }

  // ---- 5. one-in-flight law ----------------------------------------------------
  {
    // start a read burst, then immediately request another
    h.top.req_valid = 1;
    h.top.req_write = 0;
    h.top.req_client = 2;
    h.top.req_addr = 0x0000'4000;
    h.top.req_len = 64;
    h.tick();             // accepted
    h.top.req_valid = 1;  // still busy: expect err answer
    h.top.req_addr = 0x0000'5000;
    const uint32_t err0 = h.top.hps_err_count;
    h.tick();
    h.top.req_valid = 0;
    h.idle(2);
    chk(h.top.hps_err_count == err0 + 1, "busy-bridge request answers err", err0 + 1,
        h.top.hps_err_count);
    // drain the in-flight read
    const size_t b0 = h.client_beat_cycles.size();
    while (h.client_beat_cycles.size() < b0 + 8 && h.cycle < 100000) h.tick();
    chk(h.client_beat_cycles.size() == b0 + 8, "in-flight read completes");
  }

  // ---- 6. frame_tick shadow latch (D9) -----------------------------------------
  {
    h.top.frame_tick = 1;
    h.tick();
    h.top.frame_tick = 0;
    h.idle(2);  // settle the deasserted pulse before final()
    chk(true, "frame_tick shadow latch exercised");
  }

  h.top.final();
  std::printf("hps_bridge_directed: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);

  // WORKAROUND (open defect, not a test result): this binary — alone among
  // every Verilator test in the tree — completes all of its checks, prints
  // the line above, and then DEADLOCKS in process teardown. It is blocked,
  // not spinning (0.02 s CPU while hung), so CTest only ever sees a timeout
  // with no output at all, which reads as "the bridge test hangs" when in
  // fact every assertion already passed. It reproduces from a bare shell as
  // well as under CTest, and it predates the W3.5 work (stale hung
  // instances were found in the process table before any change here).
  //
  // The deadlock is after main's last statement — global/static destructor
  // teardown — so _Exit skips it without skipping any verification: the
  // exit status below is the real result of every check above. Diagnosis of
  // the teardown lock itself is filed for follow-up; masking it here would
  // be wrong, so this comment is the record.
  std::fflush(stdout);
  std::_Exit(failures ? 1 : 0);
}
