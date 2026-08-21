// hps_arbiter_directed.cpp — MEM.HPS.ARBITER composed with the REAL bridge.
//
// This is the review's Case H (§12 of the integration corrections): hold the
// bridge busy with one client's burst, present the other's request, and prove
// no request is dropped, no request is duplicated, and no response beat reaches
// the wrong client.
//
// The bridge is the real `zhao_hps_bridge`, not a model, and that matters more
// than usual here: the property under test is a protocol agreement BETWEEN the
// arbiter and the bridge, and a fake bridge is precisely the thing that agrees
// with whatever the arbiter happens to do. Two of the real bridge's behaviours
// are the whole reason the arbiter is shaped as it is, and no permissive stub
// would have either:
//
//   * a request arriving while the bridge is BUSY is a protocol violation,
//     answered with `err` -- the loser of an unarbitrated race is not made to
//     wait, it is told its transfer failed;
//   * a MALFORMED request is answered with `err` and NO grant and NO busy, so
//     an arbiter that waits only for `req_grant` waits forever.
//
// FIVE LAWS:
//
//   1. ONE OWNER AT A TIME, LATCHED. Section 2 overlaps two clients through a
//      whole burst.
//   2. A RESPONSE BEAT GOES TO THE OWNER AND NOWHERE ELSE. Every section counts
//      beats on BOTH client ports; the non-owner's must stay at zero. Section 3
//      makes each client's data unique to its own address, so a misrouted beat
//      is a wrong VALUE and not merely a wrong count.
//   3. THE SELECTED REQUEST IS HELD STABLE UNTIL GRANT. Section 4 delays the
//      HPS grant and watches the request lines.
//   4. AN `err` WITHOUT A GRANT STILL ENDS THE TRANSACTION. Section 5 sends a
//      malformed burst and then proves the port still works -- an arbiter that
//      hangs here fails the NEXT transfer, not this one.
//   5. STRICT PRIORITY, WITH THE WAITING COUNTED. Section 6.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_hps_arb_compose.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

constexpr uint32_t kBlitClient = 3;  // ZHAO_CLIENT_BLIT_DMA, for byte accounting
constexpr uint32_t kDmaClient = 1;

/** The value the fake HPS returns for a given byte address. Unique per address
 *  so a beat delivered to the wrong client is a wrong VALUE, not just a count. */
uint64_t hps_word(uint32_t addr) {
  uint64_t v = 0x9E3779B97F4A7C15ull ^ (static_cast<uint64_t>(addr) * 0x100000001B3ull);
  v ^= v >> 29;
  v *= 0xBF58476D1CE4E5B9ull;
  v ^= v >> 32;
  return v;
}

/**
 * A client that behaves like a real one: it asks for a burst, HOLDS the request
 * until the arbiter grants it, then drops it and waits for its beats before
 * asking again.
 *
 * A first version of this model simply held `valid` high forever. That is not
 * what DEBUG.FRAMEBLIT or CMD.DMA do, and under strict priority it starves the
 * low-priority client by construction -- so the test was measuring the model's
 * behaviour rather than the arbiter's.
 */
struct Client {
  bool write = false;
  uint32_t client = 0;
  uint32_t base = 0;      // first burst address
  uint32_t len = 0;
  int bursts_wanted = 0;  // how many 64-byte bursts to ask for

  // live
  bool in_flight = false;
  uint32_t addr = 0;
  // observed
  int grants = 0;
  int beats = 0;
  int errs = 0;
  bool data_ok = true;
  uint32_t next_addr = 0;
  int waited_cycles = 0;

  // write-burst state
  int wbeats_left = 0;
  int wbeats_sent = 0;

  bool wants() const { return bursts_wanted > 0 && !in_flight; }
};

/** Drives both client ports and plays the HPS. One call = one cycle. */
struct Bench {
  Vzhao_hps_arb_compose& dut;
  Client c0, c1;
  // the HPS model
  int burst_beats_left = 0;
  uint32_t burst_addr = 0;
  int grant_delay = 0;
  int grant_wait = 0;
  bool req_seen = false;
  // request-stability watch
  bool watch_stable = true;
  uint32_t last_req_addr = 0;
  uint32_t last_req_len = 0;
  bool last_req_valid = false;
  // what the bridge actually forwarded to the HPS on the write channel
  int hps_write_beats = 0;
  int hps_write_lasts = 0;
  std::vector<uint64_t> hps_write_data;

  explicit Bench(Vzhao_hps_arb_compose& d) : dut(d) {}

  void step() {
    // ---- drive the client request ports -------------------------------
    dut.c0_valid_i = c0.wants() ? 1 : 0;
    dut.c0_write_i = c0.write ? 1 : 0;
    dut.c0_client_i = c0.client;
    dut.c0_addr_i = c0.addr;
    dut.c0_len_i = c0.len;
    dut.c1_valid_i = c1.wants() ? 1 : 0;
    dut.c1_write_i = c1.write ? 1 : 0;
    dut.c1_client_i = c1.client;
    dut.c1_addr_i = c1.addr;
    dut.c1_len_i = c1.len;
    if (c0.wants()) ++c0.waited_cycles;
    if (c1.wants()) ++c1.waited_cycles;

    // ---- the HPS: read beats for an accepted burst --------------------
    dut.hps_rd_valid_i = 0;
    dut.hps_rd_data_i = 0;
    dut.hps_rd_last_i = 0;
    dut.hps_req_grant_i = 0;
    dut.eval();

    // Law 3: while the bridge holds a request out and the HPS has not taken
    // it, the address and length must not move.
    if (dut.hps_req_valid_o) {
      if (last_req_valid && (dut.hps_req_addr_o != last_req_addr ||
                             dut.hps_req_len_o != last_req_len)) {
        watch_stable = false;
      }
      last_req_valid = true;
      last_req_addr = dut.hps_req_addr_o;
      last_req_len = dut.hps_req_len_o;

      if (grant_wait < grant_delay) {
        ++grant_wait;
      } else {
        dut.hps_req_grant_i = 1;
        grant_wait = 0;
        burst_addr = dut.hps_req_addr_o;
        burst_beats_left = static_cast<int>((dut.hps_req_len_o + 7) / 8);
        last_req_valid = false;
      }
    } else {
      last_req_valid = false;
      grant_wait = 0;
    }
    dut.eval();

    if (burst_beats_left > 0 && !dut.hps_req_grant_i) {
      dut.hps_rd_valid_i = 1;
      dut.hps_rd_data_i = hps_word(burst_addr);
      burst_addr += 8;
      --burst_beats_left;
      dut.hps_rd_last_i = (burst_beats_left == 0) ? 1 : 0;
    }
    dut.eval();

    // ---- observe the client response ports ----------------------------
    if (dut.c0_grant_o) {
      ++c0.grants;
      c0.in_flight = true;
      --c0.bursts_wanted;
      if (c0.write) c0.wbeats_left = static_cast<int>((c0.len + 7) / 8);
    }
    if (dut.c1_grant_o) {
      ++c1.grants;
      c1.in_flight = true;
      --c1.bursts_wanted;
      if (c1.write) c1.wbeats_left = static_cast<int>((c1.len + 7) / 8);
    }
    if (dut.c0_beat_valid_o) {
      ++c0.beats;
      if (dut.c0_beat_data_o != hps_word(c0.next_addr)) c0.data_ok = false;
      c0.next_addr += 8;
      if (dut.c0_beat_last_o) {
        c0.in_flight = false;
        c0.addr += 64;
      }
    }
    if (dut.c1_beat_valid_o) {
      ++c1.beats;
      if (dut.c1_beat_data_o != hps_word(c1.next_addr)) c1.data_ok = false;
      c1.next_addr += 8;
      if (dut.c1_beat_last_o) {
        c1.in_flight = false;
        c1.addr += 64;
      }
    }
    if (dut.c0_beat_err_o) {
      ++c0.errs;
      c0.in_flight = false;
    }
    if (dut.c1_beat_err_o) {
      ++c1.errs;
      c1.in_flight = false;
    }

    // ---- write beats: the owner streams them after its grant ----------
    // The bridge answers a write with NOTHING, so the burst ends when the last
    // beat goes through and not on a response. An arbiter that waits for a
    // response beat hangs here, which is why this section exists at all.
    if (dut.hps_wr_valid_o) {
      ++hps_write_beats;
      hps_write_data.push_back(dut.hps_wr_data_o);
      if (dut.hps_wr_last_o) ++hps_write_lasts;
    }

    zhao::tick(dut);
    dut.eval();

    // The client presents its next beat AFTER the tick, so the value on the
    // wire during the following cycle is the one the bridge samples.
    dut.c0_wr_valid_i = 0;
    dut.c0_wr_last_i = 0;
    dut.c1_wr_valid_i = 0;
    dut.c1_wr_last_i = 0;
    if (c0.write && c0.in_flight && c0.wbeats_left > 0) {
      dut.c0_wr_valid_i = 1;
      dut.c0_wr_data_i = hps_word(c0.base + static_cast<uint32_t>(c0.wbeats_sent) * 8u);
      ++c0.wbeats_sent;
      --c0.wbeats_left;
      dut.c0_wr_last_i = (c0.wbeats_left == 0) ? 1 : 0;
      if (c0.wbeats_left == 0) c0.in_flight = false;
    }
    if (c1.write && c1.in_flight && c1.wbeats_left > 0) {
      dut.c1_wr_valid_i = 1;
      dut.c1_wr_data_i = hps_word(c1.base + static_cast<uint32_t>(c1.wbeats_sent) * 8u);
      ++c1.wbeats_sent;
      --c1.wbeats_left;
      dut.c1_wr_last_i = (c1.wbeats_left == 0) ? 1 : 0;
      if (c1.wbeats_left == 0) c1.in_flight = false;
    }
    dut.eval();
  }

  void run(int cycles) {
    for (int i = 0; i < cycles; ++i) step();
  }
};

void reset(Vzhao_hps_arb_compose& dut) {
  dut.rst_n = 0;
  dut.c0_valid_i = 0;
  dut.c1_valid_i = 0;
  dut.c0_wr_valid_i = 0;
  dut.c1_wr_valid_i = 0;
  dut.c0_wr_last_i = 0;
  dut.c1_wr_last_i = 0;
  dut.hps_req_grant_i = 0;
  dut.hps_rd_valid_i = 0;
  dut.hps_rd_last_i = 0;
  dut.frame_tick_i = 0;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
}

}  // namespace

int main() {
  Vzhao_hps_arb_compose dut;

  // ---- 1. one client alone still works ------------------------------------
  {
    reset(dut);
    Bench b(dut);
    b.c0.client = kDmaClient;
    b.c0.base = 0x1000'0000;
    b.c0.addr = 0x1000'0000;
    b.c0.next_addr = 0x1000'0000;
    b.c0.len = 64;
    b.c0.bursts_wanted = 1;
    b.run(60);
    check(b.c0.grants == 1, "1.exactly one grant", 1, static_cast<uint32_t>(b.c0.grants));
    check(b.c0.beats == 8, "1.eight beats delivered", 8, static_cast<uint32_t>(b.c0.beats));
    check(b.c0.data_ok, "1.and the data is this client's", 1, b.c0.data_ok ? 1 : 0);
    check(b.c1.beats == 0, "1.the idle client got nothing", 0,
          static_cast<uint32_t>(b.c1.beats));
    check(dut.hps_err_count_o == 0, "1.no protocol violation", 0, dut.hps_err_count_o);
  }

  // ---- 2. CASE H: contention, both orderings ------------------------------
  // Both clients want the bridge at once. Each asks for four bursts, so neither
  // can finish without the other having had turns in between.
  {
    for (int order = 0; order < 2; ++order) {
      reset(dut);
      Bench b(dut);
      b.c0.client = kDmaClient;
      b.c0.base = 0x2000'0000;
      b.c0.addr = 0x2000'0000;
      b.c0.next_addr = 0x2000'0000;
      b.c0.len = 64;
      b.c1.client = kBlitClient;
      b.c1.base = 0x3000'0000;
      b.c1.addr = 0x3000'0000;
      b.c1.next_addr = 0x3000'0000;
      b.c1.len = 64;

      // Whichever goes first gets a three-cycle head start, so the bridge is
      // genuinely BUSY when the other arrives -- which without an arbiter is
      // the case the bridge answers with `err`.
      if (order == 0) {
        b.c0.bursts_wanted = 4;
        b.run(3);
        b.c1.bursts_wanted = 4;
      } else {
        b.c1.bursts_wanted = 4;
        b.run(3);
        b.c0.bursts_wanted = 4;
      }
      b.run(400);

      char nm[96];
      std::snprintf(nm, sizeof nm, "2.order %d: c0 completed all four bursts", order);
      check(b.c0.beats == 32, nm, 32, static_cast<uint32_t>(b.c0.beats));
      std::snprintf(nm, sizeof nm, "2.order %d: c1 completed all four bursts", order);
      check(b.c1.beats == 32, nm, 32, static_cast<uint32_t>(b.c1.beats));

      // LAW 2, the one that corrupts rather than stalls: every beat carried the
      // data of the address ITS OWN client asked for. A misrouted beat is a
      // wrong VALUE here, not merely a wrong count.
      std::snprintf(nm, sizeof nm, "2.order %d: c0's data is c0's", order);
      check(b.c0.data_ok, nm, 1, b.c0.data_ok ? 1 : 0);
      std::snprintf(nm, sizeof nm, "2.order %d: c1's data is c1's", order);
      check(b.c1.data_ok, nm, 1, b.c1.data_ok ? 1 : 0);

      // Nobody was told their transfer failed. Without the arbiter the loser of
      // the race gets exactly this.
      std::snprintf(nm, sizeof nm, "2.order %d: c0 saw no error", order);
      check(b.c0.errs == 0, nm, 0, static_cast<uint32_t>(b.c0.errs));
      std::snprintf(nm, sizeof nm, "2.order %d: c1 saw no error", order);
      check(b.c1.errs == 0, nm, 0, static_cast<uint32_t>(b.c1.errs));
      std::snprintf(nm, sizeof nm, "2.order %d: the bridge logged NO violation", order);
      check(dut.hps_err_count_o == 0, nm, 0, dut.hps_err_count_o);

      // No dropped and no duplicated requests: four grants, four bursts.
      std::snprintf(nm, sizeof nm, "2.order %d: c0 grants match bursts", order);
      check(b.c0.grants == 4, nm, 4, static_cast<uint32_t>(b.c0.grants));
      std::snprintf(nm, sizeof nm, "2.order %d: c1 grants match bursts", order);
      check(b.c1.grants == 4, nm, 4, static_cast<uint32_t>(b.c1.grants));
    }
  }

  // ---- 3. adjacent addresses: a split burst cannot look correct -----------
  // The sharpest form of law 2. The two clients read the two halves of the same
  // 128 bytes, so a beat delivered to the wrong client carries a value that is
  // plausible in shape and wrong in fact.
  {
    reset(dut);
    Bench b(dut);
    b.c0.client = kDmaClient;
    b.c0.base = 0x4000'0000;
    b.c0.addr = 0x4000'0000;
    b.c0.next_addr = 0x4000'0000;
    b.c0.len = 64;
    b.c0.bursts_wanted = 3;
    b.c1.client = kBlitClient;
    b.c1.base = 0x4000'0040;
    b.c1.addr = 0x4000'0040;
    b.c1.next_addr = 0x4000'0040;
    b.c1.len = 64;
    b.c1.bursts_wanted = 3;
    b.run(400);
    check(b.c0.data_ok && b.c1.data_ok, "3.adjacent bursts stay separate", 1,
          (b.c0.data_ok && b.c1.data_ok) ? 1 : 0);
    check(b.c0.beats == 24 && b.c1.beats == 24, "3.and both completed", 1,
          (b.c0.beats == 24 && b.c1.beats == 24) ? 1 : 0);
    check(dut.hps_err_count_o == 0, "3.no violation", 0, dut.hps_err_count_o);
  }

  // ---- 4. law 3: the bridge request is ONE cycle, whatever the HPS does ---
  // The HPS is made slow to take the request. That does NOT license the arbiter
  // to hold its request on the bridge: the bridge has already accepted it and a
  // second cycle of `valid` is a request-while-busy, which is a violation. The
  // `hps_err_count_o` check is what catches that, and it is the reason this
  // section exists.
  {
    for (int delay : {1, 5, 17}) {
      reset(dut);
      Bench b(dut);
      b.grant_delay = delay;
      b.c0.client = kDmaClient;
      b.c0.base = 0x5000'0000;
      b.c0.addr = 0x5000'0000;
      b.c0.next_addr = 0x5000'0000;
      b.c0.len = 64;
      b.c0.bursts_wanted = 2;
      b.c1.client = kBlitClient;
      b.c1.base = 0x6000'0000;
      b.c1.addr = 0x6000'0000;
      b.c1.next_addr = 0x6000'0000;
      b.c1.len = 64;
      b.c1.bursts_wanted = 2;
      b.run(600);
      char nm[96];
      std::snprintf(nm, sizeof nm, "4.hps delay %d: request held stable to the HPS", delay);
      check(b.watch_stable, nm, 1, b.watch_stable ? 1 : 0);
      std::snprintf(nm, sizeof nm, "4.hps delay %d: both served", delay);
      check(b.c0.beats == 16 && b.c1.beats == 16, nm, 1,
            (b.c0.beats == 16 && b.c1.beats == 16) ? 1 : 0);
      std::snprintf(nm, sizeof nm, "4.hps delay %d: data correct", delay);
      check(b.c0.data_ok && b.c1.data_ok, nm, 1,
            (b.c0.data_ok && b.c1.data_ok) ? 1 : 0);
      std::snprintf(nm, sizeof nm, "4.hps delay %d: NO protocol violation", delay);
      check(dut.hps_err_count_o == 0, nm, 0, dut.hps_err_count_o);
    }
  }

  // ---- 5. law 4: a malformed burst must not wedge the port ---------------
  // The real bridge answers a malformed request with err|last, NO grant and NO
  // busy. An arbiter that waits for the grant holds the port forever -- and the
  // symptom is not this transfer failing, it is the NEXT one never happening.
  {
    for (int shape = 0; shape < 2; ++shape) {
      reset(dut);
      Bench b(dut);
      b.c1.client = kBlitClient;
      // shape 0: length zero. shape 1: a misaligned address.
      b.c1.base = (shape == 0) ? 0x7000'0000u : 0x7000'0004u;
      b.c1.addr = b.c1.base;
      b.c1.next_addr = b.c1.base;
      b.c1.len = (shape == 0) ? 0u : 64u;
      b.c1.bursts_wanted = 1;
      b.run(16);

      char nm[96];
      std::snprintf(nm, sizeof nm, "5.shape %d: the malformed client is told", shape);
      check(b.c1.errs > 0, nm, 1, static_cast<uint32_t>(b.c1.errs));
      std::snprintf(nm, sizeof nm, "5.shape %d: and c0 is not blamed for it", shape);
      check(b.c0.errs == 0, nm, 0, static_cast<uint32_t>(b.c0.errs));

      // THE REAL ASSERTION: the port must still work afterwards.
      b.c1.bursts_wanted = 0;
      b.c0.client = kDmaClient;
      b.c0.base = 0x8000'0000;
      b.c0.addr = 0x8000'0000;
      b.c0.next_addr = 0x8000'0000;
      b.c0.len = 64;
      b.c0.bursts_wanted = 2;
      b.run(200);
      std::snprintf(nm, sizeof nm, "5.shape %d: the port still serves afterwards", shape);
      check(b.c0.beats == 16, nm, 16, static_cast<uint32_t>(b.c0.beats));
      std::snprintf(nm, sizeof nm, "5.shape %d: with correct data", shape);
      check(b.c0.data_ok, nm, 1, b.c0.data_ok ? 1 : 0);
    }
  }

  // ---- 6. law 5: strict priority, and what it actually guarantees --------
  // Strict priority guarantees that client 0 never waits behind client 1 for a
  // NEW burst. It does NOT guarantee client 1 is ever served: a client 0 that
  // asks continuously starves it forever.
  //
  // That is the policy the review asks for -- "a debug blit is not game-facing
  // and may wait" -- and in practice CMD.DMA asks once per frame, so the
  // starvation window is bounded by how often it asks and not by this arbiter.
  // But the arbiter cannot see that, and a policy whose safety depends on
  // somebody else's request pattern should be VISIBLE rather than assumed. That
  // is what `c1_wait_cycles_o` is for, and this section pins both halves: the
  // priority is real, and so is its cost.
  {
    reset(dut);
    Bench b(dut);
    b.c0.client = kDmaClient;
    b.c0.base = 0x9000'0000;
    b.c0.addr = 0x9000'0000;
    b.c0.next_addr = 0x9000'0000;
    b.c0.len = 64;
    b.c0.bursts_wanted = 6;
    b.c1.client = kBlitClient;
    b.c1.base = 0xA000'0000;
    b.c1.addr = 0xA000'0000;
    b.c1.next_addr = 0xA000'0000;
    b.c1.len = 64;
    b.c1.bursts_wanted = 6;
    b.run(700);
    check(b.c0.beats == 48 && b.c1.beats == 48, "6.both finish once each stops asking", 1,
          (b.c0.beats == 48 && b.c1.beats == 48) ? 1 : 0);
    check(dut.c1_wait_cycles_o > 0, "6.client 1's waiting is counted", 1,
          dut.c1_wait_cycles_o);
    check(dut.c0_bursts_o == 6 && dut.c1_bursts_o == 6, "6.both burst counters agree", 6,
          dut.c1_bursts_o);
    check(b.c1.waited_cycles > b.c0.waited_cycles,
          "6.and the LOW priority client waited longer",
          static_cast<uint32_t>(b.c0.waited_cycles),
          static_cast<uint32_t>(b.c1.waited_cycles));
    std::printf("priority: c0 %d grants (waited %d), c1 %d grants (waited %d), "
                "counter %u\n",
                b.c0.grants, b.c0.waited_cycles, b.c1.grants, b.c1.waited_cycles,
                dut.c1_wait_cycles_o);
  }

  // ---- 6c. A PULSING CLIENT, which is what CMD.DMA actually is -----------
  // The two clients do not agree on how long a request stays up.
  // DEBUG.FRAMEBLIT HOLDS its request until granted; CMD.DMA sets `hps_req_v`
  // in one state and clears it by a default assignment the next cycle -- a
  // single-cycle pulse. An arbiter that re-reads the client port after deciding
  // works with the first and silently loses every request from the second.
  //
  // Nothing found this until the block was actually wired into the shell, which
  // is the argument for wiring it early rather than at the end.
  {
    reset(dut);
    Bench b(dut);
    // Drive the port by hand: valid for exactly ONE cycle, then gone.
    dut.c0_valid_i = 1;
    dut.c0_write_i = 0;
    dut.c0_client_i = kDmaClient;
    dut.c0_addr_i = 0xF000'0000;
    dut.c0_len_i = 64;
    dut.eval();
    zhao::tick(dut);
    dut.c0_valid_i = 0;
    dut.eval();

    // From here the harness plays the HPS only; the client says nothing more.
    b.c0.client = kDmaClient;
    b.c0.base = 0xF000'0000;
    b.c0.addr = 0xF000'0000;
    b.c0.next_addr = 0xF000'0000;
    b.c0.len = 64;
    b.c0.bursts_wanted = 0;   // it will not ask again
    b.run(80);

    check(b.c0.beats == 8, "6c.a one-cycle request is not lost", 8,
          static_cast<uint32_t>(b.c0.beats));
    check(b.c0.data_ok, "6c.and it fetched the right address", 1, b.c0.data_ok ? 1 : 0);
    check(dut.hps_err_count_o == 0, "6c.no protocol violation", 0, dut.hps_err_count_o);
  }

  // ---- 7. A WRITE BURST, which ends differently ---------------------------
  // The bridge answers a write with NOTHING -- it forwards the beats and clears
  // `busy` on `wr_last`. An arbiter that ends a burst on `rsp.last` therefore
  // holds the port FOREVER on the first write it ever arbitrates.
  //
  // Nothing in Phase 2 writes to HPS DDR. This section exists because the
  // mutation sweep found the path untested: a mutation routing write data from
  // the WRONG client survived, since no test issued a write at all. An untested
  // path with a latent hang is worse than an absent one, because it looks
  // finished.
  {
    reset(dut);
    Bench b(dut);
    b.c1.client = kBlitClient;
    b.c1.write = true;
    b.c1.base = 0xD000'0000;
    b.c1.addr = 0xD000'0000;
    b.c1.len = 64;
    b.c1.bursts_wanted = 1;
    b.run(80);

    check(b.c1.grants == 1, "7.the write burst was granted", 1,
          static_cast<uint32_t>(b.c1.grants));
    check(b.hps_write_beats == 8, "7.eight write beats reached the HPS", 8,
          static_cast<uint32_t>(b.hps_write_beats));
    check(b.hps_write_lasts == 1, "7.with exactly one `last`", 1,
          static_cast<uint32_t>(b.hps_write_lasts));

    // The DATA must be the writing client's. This is the assertion the
    // surviving mutation had nothing to fail against.
    bool wdata_ok = (b.hps_write_data.size() == 8);
    for (size_t i = 0; i < b.hps_write_data.size() && wdata_ok; ++i) {
      if (b.hps_write_data[i] != hps_word(0xD000'0000u + static_cast<uint32_t>(i) * 8u)) {
        wdata_ok = false;
      }
    }
    check(wdata_ok, "7.and it is the writing client's data", 1, wdata_ok ? 1 : 0);
    check(dut.hps_err_count_o == 0, "7.no protocol violation", 0, dut.hps_err_count_o);

    // THE HANG TEST: the port must still work afterwards. An arbiter waiting
    // for a response that never comes fails here, not above.
    b.c0.client = kDmaClient;
    b.c0.base = 0xE000'0000;
    b.c0.addr = 0xE000'0000;
    b.c0.next_addr = 0xE000'0000;
    b.c0.len = 64;
    b.c0.bursts_wanted = 1;
    b.run(120);
    check(b.c0.beats == 8, "7.the port is NOT wedged by the write", 8,
          static_cast<uint32_t>(b.c0.beats));
    check(b.c0.data_ok, "7.and the following read is correct", 1, b.c0.data_ok ? 1 : 0);
  }

  // ---- 7b. a write and a read contending ----------------------------------
  {
    reset(dut);
    Bench b(dut);
    b.c0.client = kDmaClient;
    b.c0.base = 0xE100'0000;
    b.c0.addr = 0xE100'0000;
    b.c0.next_addr = 0xE100'0000;
    b.c0.len = 64;
    b.c0.bursts_wanted = 3;
    b.c1.client = kBlitClient;
    b.c1.write = true;
    b.c1.base = 0xE200'0000;
    b.c1.addr = 0xE200'0000;
    b.c1.len = 64;
    b.c1.bursts_wanted = 3;
    b.run(500);
    check(b.c0.beats == 24, "7b.the reader got every beat", 24,
          static_cast<uint32_t>(b.c0.beats));
    check(b.c0.data_ok, "7b.and its data is its own", 1, b.c0.data_ok ? 1 : 0);
    check(b.c1.grants == 3, "7b.the writer got every burst", 3,
          static_cast<uint32_t>(b.c1.grants));
    check(b.c1.beats == 0, "7b.and the writer received no read beats", 0,
          static_cast<uint32_t>(b.c1.beats));
    check(b.hps_write_lasts == 3, "7b.three write bursts completed", 3,
          static_cast<uint32_t>(b.hps_write_lasts));
    check(dut.hps_err_count_o == 0, "7b.no protocol violation", 0, dut.hps_err_count_o);
  }

  // ---- 6b. the starvation is REAL, and this is what it looks like --------
  // Client 0 never stops asking. Client 1 is never served. Asserting this is
  // not endorsing it -- it is refusing to let a policy consequence stay
  // undocumented, so that if the shell ever gives CMD.DMA a continuous request
  // pattern the failure is one somebody predicted.
  {
    reset(dut);
    Bench b(dut);
    b.c0.client = kDmaClient;
    b.c0.base = 0xB000'0000;
    b.c0.addr = 0xB000'0000;
    b.c0.next_addr = 0xB000'0000;
    b.c0.len = 64;
    b.c0.bursts_wanted = 1000;   // never stops
    b.c1.client = kBlitClient;
    b.c1.base = 0xC000'0000;
    b.c1.addr = 0xC000'0000;
    b.c1.next_addr = 0xC000'0000;
    b.c1.len = 64;
    b.c1.bursts_wanted = 1;
    b.run(600);
    check(b.c0.grants > 10, "6b.the high-priority client runs freely", 1,
          static_cast<uint32_t>(b.c0.grants));
    check(b.c1.grants == 0, "6b.and the low-priority client is STARVED", 0,
          static_cast<uint32_t>(b.c1.grants));
    check(dut.c1_wait_cycles_o > 500, "6b.which the wait counter makes visible", 500,
          dut.c1_wait_cycles_o);
  }

  dut.final();
  return zhao::report_and_exit("hps_arbiter_directed");
}
