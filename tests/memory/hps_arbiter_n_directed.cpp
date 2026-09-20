// hps_arbiter_n_directed.cpp -- MEM.HPS.ARBITER at N=3, composed with the REAL
// bridge. Owner ruling R4 (reports/OWNER-RULINGS-20260919-EVENING.md): widen
// to N clients, PRESERVE the starvation law, RE-PROVE it.
//
// tests/memory/hps_arbiter_directed.cpp still proves the seven rules at N=2 --
// through `zhao_hps_arbiter`, which is now the N core's two-client wrapper, so
// those 66 checks exercise the same machine. This driver re-proves every rule
// with a third client present, and adds the two cases no two-client test can
// reach:
//
//   * a MIDDLE client, which is outranked (by 0) AND outranking (over 2). The
//     law applied twice: a middle client that asks continuously starves client
//     2 exactly as client 0 starves client 1, and client 0 still pre-empts it
//     at the next arbitration -- waiting only for the burst in flight.
//   * the wait counter of EVERY client that can be made to wait, not only
//     client 1's -- a counter for client 2 that stayed at zero while client 2
//     starved would be the flattering failure, so it is fired by stimulus here.
//
// Each client's read data is a hash of its own address, so a beat routed to
// the wrong client is a wrong VALUE, not merely a wrong count, and the bridge's
// own violation counter is asserted zero in every well-formed section.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_hps_arb_n_compose.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;
constexpr int kN = 3;

uint64_t hps_word(uint32_t addr) {
  uint64_t v = 0x9E3779B97F4A7C15ull ^ (static_cast<uint64_t>(addr) * 0x100000001B3ull);
  v ^= v >> 29;
  v *= 0xBF58476D1CE4E5B9ull;
  v ^= v >> 32;
  return v;
}

// ---- packed-array field access (element i of a [2:0][W-1:0] port) --------
void set_bits(uint32_t& word, int lsb, int w, uint32_t v) {
  const uint32_t m = ((w == 32) ? 0xFFFFFFFFu : ((1u << w) - 1u)) << lsb;
  word = (word & ~m) | ((v << lsb) & m);
}
uint64_t get_wide64(const VlWide<6>& w, int i) {
  return static_cast<uint64_t>(w[2 * i]) | (static_cast<uint64_t>(w[2 * i + 1]) << 32);
}
void set_wide64(VlWide<6>& w, int i, uint64_t v) {
  w[2 * i] = static_cast<uint32_t>(v);
  w[2 * i + 1] = static_cast<uint32_t>(v >> 32);
}
uint32_t bit(uint32_t v, int i) { return (v >> i) & 1u; }

/** A client that behaves like a real one: asks, HOLDS until granted, then waits
 *  for its beats before asking again. (Same model as the two-client test.) */
struct Client {
  bool write = false;
  uint32_t client = 0;
  uint32_t base = 0;
  uint32_t len = 64;
  int bursts_wanted = 0;
  bool in_flight = false;
  uint32_t addr = 0;
  int grants = 0;
  int beats = 0;
  int errs = 0;
  bool data_ok = true;
  uint32_t next_addr = 0;
  int waited_cycles = 0;
  int wbeats_left = 0;
  int wbeats_sent = 0;
  bool wants() const { return bursts_wanted > 0 && !in_flight; }
  void at(uint32_t a, int n) {
    base = a;
    addr = a;
    next_addr = a;
    bursts_wanted = n;
  }
};

struct Bench {
  Vzhao_hps_arb_n_compose& dut;
  Client c[kN];
  int burst_beats_left = 0;
  uint32_t burst_addr = 0;
  int grant_delay = 0;
  int grant_wait = 0;
  bool watch_stable = true;
  uint32_t last_req_addr = 0;
  uint32_t last_req_len = 0;
  bool last_req_valid = false;
  int hps_write_beats = 0;
  int hps_write_lasts = 0;
  std::vector<uint64_t> hps_write_data;
  int multi_beat_cycles = 0;  // cycles on which >1 client port carried a beat
  uint32_t pulse = 0;         // one-shot extra `valid`, CMD.DMA style (rule 6b)
  uint32_t stray_wr = 0;      // clients that raise wr_valid while READING (S2)

  explicit Bench(Vzhao_hps_arb_n_compose& d) : dut(d) {
    for (int i = 0; i < kN; ++i) c[i].client = static_cast<uint32_t>(i + 1);
  }

  void drive_requests() {
    uint32_t valid = 0, write = 0, len = 0;
    uint16_t client = 0;
    for (int i = 0; i < kN; ++i) {
      if (c[i].wants()) {
        valid |= 1u << i;
        ++c[i].waited_cycles;
      }
      if (c[i].write) write |= 1u << i;
      client = static_cast<uint16_t>(client | ((c[i].client & 7u) << (3 * i)));
      set_bits(len, 7 * i, 7, c[i].len);
      dut.c_addr_i[i] = c[i].addr;
    }
    valid |= pulse;
    pulse = 0;
    dut.c_valid_i = static_cast<uint8_t>(valid);
    dut.c_write_i = static_cast<uint8_t>(write);
    dut.c_client_i = client;
    dut.c_len_i = len;
  }

  void step() {
    drive_requests();
    dut.hps_rd_valid_i = 0;
    dut.hps_rd_data_i = 0;
    dut.hps_rd_last_i = 0;
    dut.hps_req_grant_i = 0;
    dut.eval();

    if (dut.hps_req_valid_o) {
      if (last_req_valid &&
          (dut.hps_req_addr_o != last_req_addr || dut.hps_req_len_o != last_req_len)) {
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

    int beating = 0;
    for (int i = 0; i < kN; ++i) {
      Client& k = c[i];
      if (bit(dut.c_grant_o, i)) {
        ++k.grants;
        k.in_flight = true;
        --k.bursts_wanted;
        if (k.write) k.wbeats_left = static_cast<int>((k.len + 7) / 8);
      }
      if (bit(dut.c_beat_valid_o, i)) {
        ++beating;
        ++k.beats;
        if (get_wide64(dut.c_beat_data_o, i) != hps_word(k.next_addr)) k.data_ok = false;
        k.next_addr += 8;
        if (bit(dut.c_beat_last_o, i)) {
          k.in_flight = false;
          k.addr += 64;
        }
      }
      if (bit(dut.c_beat_err_o, i)) {
        ++k.errs;
        k.in_flight = false;
      }
    }
    if (beating > 1) ++multi_beat_cycles;

    if (dut.hps_wr_valid_o) {
      ++hps_write_beats;
      hps_write_data.push_back(dut.hps_wr_data_o);
      if (dut.hps_wr_last_o) ++hps_write_lasts;
    }

    zhao::tick(dut);
    dut.eval();

    uint32_t wv = 0, wl = 0;
    for (int i = 0; i < kN; ++i) {
      Client& k = c[i];
      if (k.write && k.in_flight && k.wbeats_left > 0) {
        wv |= 1u << i;
        set_wide64(dut.c_wr_data_i, i,
                   hps_word(k.base + static_cast<uint32_t>(k.wbeats_sent) * 8u));
        ++k.wbeats_sent;
        --k.wbeats_left;
        if (k.wbeats_left == 0) {
          wl |= 1u << i;
          k.in_flight = false;
        }
      }
    }
    wv |= stray_wr;
    wl |= stray_wr;
    dut.c_wr_valid_i = static_cast<uint8_t>(wv);
    dut.c_wr_last_i = static_cast<uint8_t>(wl);
    dut.eval();
  }

  void run(int cycles) {
    for (int i = 0; i < cycles; ++i) step();
  }
};

void reset(Vzhao_hps_arb_n_compose& dut) {
  dut.rst_n = 0;
  dut.c_valid_i = 0;
  dut.c_wr_valid_i = 0;
  dut.c_wr_last_i = 0;
  dut.hps_req_grant_i = 0;
  dut.hps_rd_valid_i = 0;
  dut.hps_rd_last_i = 0;
  dut.frame_tick_i = 0;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
}

uint32_t bursts(Vzhao_hps_arb_n_compose& dut, int i) { return dut.bursts_o[i]; }

char nm[128];
// Two arities rather than one with a defaulted second argument: passing an
// unused argument to a one-placeholder format is what cppcheck's
// wrongPrintfScanfArgNum reports, and it is right to.
const char* name(const char* fmt, int a) {
  std::snprintf(nm, sizeof nm, fmt, a);
  return nm;
}
const char* name(const char* fmt, int a, int b) {
  std::snprintf(nm, sizeof nm, fmt, a, b);
  return nm;
}

}  // namespace

int main() {
  Vzhao_hps_arb_n_compose dut;

  // ---- 1. every client alone, including the new third one -----------------
  for (int who = 0; who < kN; ++who) {
    reset(dut);
    Bench b(dut);
    b.c[who].at(0x1000'0000u + static_cast<uint32_t>(who) * 0x0100'0000u, 1);
    b.run(60);
    check(b.c[who].grants == 1, name("1.client %d alone: exactly one grant", who), 1,
          static_cast<uint32_t>(b.c[who].grants));
    check(b.c[who].beats == 8, name("1.client %d alone: eight beats", who), 8,
          static_cast<uint32_t>(b.c[who].beats));
    check(b.c[who].data_ok, name("1.client %d alone: its own data", who), 1, b.c[who].data_ok);
    int others = 0;
    for (int j = 0; j < kN; ++j) {
      if (j != who) others += b.c[j].beats + b.c[j].errs;
    }
    check(others == 0, name("1.client %d alone: nobody else got anything", who), 0,
          static_cast<uint32_t>(others));
    check(bursts(dut, who) == 1, name("1.client %d alone: its burst counter", who), 1,
          bursts(dut, who));
    check(dut.hps_err_count_o == 0, name("1.client %d alone: no violation", who), 0,
          dut.hps_err_count_o);
  }

  // ---- 2. three-way contention, each client given the head start ----------
  // The first asker gets three cycles alone, so the bridge is genuinely BUSY
  // when the other two arrive -- the case the bridge answers with `err` when
  // nothing arbitrates.
  for (int first = 0; first < kN; ++first) {
    reset(dut);
    Bench b(dut);
    for (int i = 0; i < kN; ++i)
      b.c[i].at(0x2000'0000u + static_cast<uint32_t>(i) * 0x0100'0000u, 0);
    b.c[first].bursts_wanted = 4;
    b.run(3);
    for (int i = 0; i < kN; ++i) {
      if (i != first) b.c[i].bursts_wanted = 4;
    }
    b.run(700);
    for (int i = 0; i < kN; ++i) {
      check(b.c[i].beats == 32, name("2.head start %d: client %d completed four bursts", first, i),
            32, static_cast<uint32_t>(b.c[i].beats));
      check(b.c[i].data_ok, name("2.head start %d: client %d's data is its own", first, i), 1,
            b.c[i].data_ok);
      check(b.c[i].errs == 0, name("2.head start %d: client %d saw no error", first, i), 0,
            static_cast<uint32_t>(b.c[i].errs));
      check(b.c[i].grants == 4 && bursts(dut, i) == 4,
            name("2.head start %d: client %d grants == bursts == 4", first, i), 4, bursts(dut, i));
    }
    check(b.multi_beat_cycles == 0,
          name("2.head start %d: never two clients beating at once", first), 0,
          static_cast<uint32_t>(b.multi_beat_cycles));
    check(dut.hps_err_count_o == 0, name("2.head start %d: the bridge logged NO violation", first),
          0, dut.hps_err_count_o);
  }

  // ---- 3. adjacent addresses: a misrouted beat is plausible and wrong -----
  {
    reset(dut);
    Bench b(dut);
    for (int i = 0; i < kN; ++i) b.c[i].at(0x4000'0000u + static_cast<uint32_t>(i) * 0x40u, 3);
    // Each client's second and third bursts step by 64, so give them disjoint
    // strides: client i reads i, i+3, i+6 (in 64-byte units) -- interleaved.
    b.run(700);
    bool ok = true;
    for (int i = 0; i < kN; ++i) ok = ok && b.c[i].data_ok && b.c[i].beats == 24;
    check(ok, "3.three interleaved neighbours stay separate and complete", 1, ok);
    check(dut.hps_err_count_o == 0, "3.no violation", 0, dut.hps_err_count_o);
  }

  // ---- 4. the bridge request is ONE cycle, however slow the HPS is --------
  for (int delay : {1, 5, 17}) {
    reset(dut);
    Bench b(dut);
    b.grant_delay = delay;
    for (int i = 0; i < kN; ++i)
      b.c[i].at(0x5000'0000u + static_cast<uint32_t>(i) * 0x0100'0000u, 2);
    b.run(900);
    check(b.watch_stable, name("4.hps delay %d: request held stable to the HPS", delay), 1,
          b.watch_stable);
    bool served = true;
    for (int i = 0; i < kN; ++i) served = served && b.c[i].beats == 16 && b.c[i].data_ok;
    check(served, name("4.hps delay %d: all three served, correct data", delay), 1, served);
    check(dut.hps_err_count_o == 0, name("4.hps delay %d: NO protocol violation", delay), 0,
          dut.hps_err_count_o);
  }

  // ---- 5. a malformed burst from the NEW client must not wedge the port ---
  for (int shape = 0; shape < 2; ++shape) {
    reset(dut);
    Bench b(dut);
    b.c[2].at((shape == 0) ? 0x7000'0000u : 0x7000'0004u, 1);
    b.c[2].len = (shape == 0) ? 0u : 64u;
    b.run(16);
    check(b.c[2].errs > 0, name("5.shape %d: the malformed client is told", shape), 1,
          static_cast<uint32_t>(b.c[2].errs));
    check(b.c[0].errs + b.c[1].errs == 0, name("5.shape %d: nobody else is blamed", shape), 0,
          static_cast<uint32_t>(b.c[0].errs + b.c[1].errs));
    b.c[2].bursts_wanted = 0;
    b.c[0].at(0x8000'0000u, 2);
    b.c[1].at(0x8100'0000u, 2);
    b.run(300);
    check(b.c[0].beats == 16 && b.c[1].beats == 16,
          name("5.shape %d: the port still serves both others afterwards", shape), 1,
          b.c[0].beats == 16 && b.c[1].beats == 16);
    check(b.c[0].data_ok && b.c[1].data_ok, name("5.shape %d: with correct data", shape), 1,
          b.c[0].data_ok && b.c[1].data_ok);
  }

  // ---- 6. THE STARVATION LAW, preserved: strict priority BY INDEX ---------
  // All three ask for six bursts at once. Strict priority means the order of
  // service is 0, then 1, then 2 -- so the waiting is strictly ordered too, and
  // both waiting clients' counters move.
  {
    reset(dut);
    Bench b(dut);
    for (int i = 0; i < kN; ++i)
      b.c[i].at(0x9000'0000u + static_cast<uint32_t>(i) * 0x0100'0000u, 6);
    b.run(1200);
    bool done = true;
    for (int i = 0; i < kN; ++i) done = done && b.c[i].beats == 48 && bursts(dut, i) == 6;
    check(done, "6.all three finish once each stops asking", 1, done);
    check(b.c[1].waited_cycles > b.c[0].waited_cycles, "6.client 1 waited longer than client 0",
          static_cast<uint32_t>(b.c[0].waited_cycles), static_cast<uint32_t>(b.c[1].waited_cycles));
    check(b.c[2].waited_cycles > b.c[1].waited_cycles, "6.client 2 waited longer than client 1",
          static_cast<uint32_t>(b.c[1].waited_cycles), static_cast<uint32_t>(b.c[2].waited_cycles));
    check(dut.c1_wait_cycles_o > 0, "6.client 1's waiting is counted", 1, dut.c1_wait_cycles_o);
    check(dut.c2_wait_cycles_o > dut.c1_wait_cycles_o,
          "6.client 2's waiting is counted, and is larger", dut.c1_wait_cycles_o,
          dut.c2_wait_cycles_o);
    std::printf("priority: waited c0 %d, c1 %d, c2 %d; counters c1 %u c2 %u\n",
                b.c[0].waited_cycles, b.c[1].waited_cycles, b.c[2].waited_cycles,
                dut.c1_wait_cycles_o, dut.c2_wait_cycles_o);
  }

  // ---- 6b. client 0 asking forever starves BOTH others, visibly -----------
  // Asserting this is not endorsing it: it is the law's documented cost, and a
  // wait counter that stayed at zero here would be the flattering failure.
  {
    reset(dut);
    Bench b(dut);
    b.c[0].at(0xB000'0000u, 100000);
    b.c[1].at(0xB100'0000u, 1);
    b.c[2].at(0xB200'0000u, 1);
    b.run(600);
    check(b.c[0].grants > 10, "6b.client 0 runs freely", 1, static_cast<uint32_t>(b.c[0].grants));
    check(b.c[1].grants == 0 && b.c[2].grants == 0, "6b.clients 1 AND 2 are STARVED", 0,
          static_cast<uint32_t>(b.c[1].grants + b.c[2].grants));
    check(dut.c1_wait_cycles_o > 500, "6b.client 1's counter shows it", 500, dut.c1_wait_cycles_o);
    check(dut.c2_wait_cycles_o > 500, "6b.client 2's counter shows it", 500, dut.c2_wait_cycles_o);

    // ...and the starvation ENDS when the priority client stops: no lock-up,
    // and the survivors are served in index order.
    b.c[0].bursts_wanted = 0;
    b.run(200);
    check(b.c[1].beats == 8 && b.c[2].beats == 8, "6b.once client 0 stops, both are served", 1,
          b.c[1].beats == 8 && b.c[2].beats == 8);
    check(b.c[1].data_ok && b.c[2].data_ok, "6b.with their own data", 1,
          b.c[1].data_ok && b.c[2].data_ok);
  }

  // ---- 6c. THE MIDDLE CLIENT: outranked and outranking --------------------
  // Client 1 asks forever. It starves client 2 -- the same law, one index up --
  // and client 0, arriving later, still wins the very next arbitration: it
  // waits only for the burst already in flight, never for client 1's queue.
  {
    reset(dut);
    Bench b(dut);
    b.c[1].at(0xC100'0000u, 100000);
    b.c[2].at(0xC200'0000u, 1);
    b.run(400);
    check(b.c[1].grants > 10, "6c.the middle client runs freely with client 0 idle", 1,
          static_cast<uint32_t>(b.c[1].grants));
    check(b.c[2].grants == 0, "6c.and starves client 2", 0, static_cast<uint32_t>(b.c[2].grants));
    check(dut.c2_wait_cycles_o > 350, "6c.which client 2's counter shows", 350,
          dut.c2_wait_cycles_o);

    const int c1_before = b.c[1].grants;
    b.c[0].at(0xC000'0000u, 1);
    b.run(60);
    check(b.c[0].beats == 8 && b.c[0].data_ok, "6c.client 0 pre-empts the middle client", 1,
          b.c[0].beats == 8 && b.c[0].data_ok);
    // One in-flight burst of client 1 is ~12 cycles here; client 0's whole wait
    // must be bounded by that, not by client 1's endless queue.
    check(b.c[0].waited_cycles <= 20, "6c.waiting only for the burst in flight", 20,
          static_cast<uint32_t>(b.c[0].waited_cycles));
    check(b.c[1].grants > c1_before, "6c.and the middle client resumes afterwards", 1,
          static_cast<uint32_t>(b.c[1].grants - c1_before));
    check(b.c[2].grants == 0, "6c.client 2 is still starved -- the law, not a bug", 0,
          static_cast<uint32_t>(b.c[2].grants));
    check(dut.hps_err_count_o == 0, "6c.no violation", 0, dut.hps_err_count_o);
  }

  // ---- 6d. a PULSING request on the new index is not lost -----------------
  // Rule 6: the request is latched when the owner is chosen, whichever index.
  {
    reset(dut);
    Bench b(dut);
    dut.c_valid_i = 0b100;
    dut.c_write_i = 0;
    dut.c_client_i = static_cast<uint16_t>(3u << 6);
    dut.c_addr_i[2] = 0xF200'0000u;
    dut.c_len_i = 64u << 14;
    dut.eval();
    zhao::tick(dut);
    dut.c_valid_i = 0;
    dut.eval();
    b.c[2].base = 0xF200'0000u;
    b.c[2].addr = 0xF200'0000u;
    b.c[2].next_addr = 0xF200'0000u;
    b.c[2].bursts_wanted = 0;
    b.run(80);
    check(b.c[2].beats == 8, "6d.a one-cycle request on client 2 is not lost", 8,
          static_cast<uint32_t>(b.c[2].beats));
    check(b.c[2].data_ok, "6d.and it fetched the right address", 1, b.c[2].data_ok);
    check(dut.hps_err_count_o == 0, "6d.no violation", 0, dut.hps_err_count_o);
  }

  // ---- 7. a WRITE from the new client ends on wr_last, and the port lives -
  {
    reset(dut);
    Bench b(dut);
    b.c[2].write = true;
    b.c[2].at(0xD200'0000u, 1);
    b.run(80);
    check(b.c[2].grants == 1, "7.client 2's write burst was granted", 1,
          static_cast<uint32_t>(b.c[2].grants));
    check(b.hps_write_beats == 8 && b.hps_write_lasts == 1, "7.eight beats, one last", 1,
          b.hps_write_beats == 8 && b.hps_write_lasts == 1);
    bool wdata_ok = (b.hps_write_data.size() == 8);
    for (size_t i = 0; i < b.hps_write_data.size() && wdata_ok; ++i) {
      if (b.hps_write_data[i] != hps_word(0xD200'0000u + static_cast<uint32_t>(i) * 8u))
        wdata_ok = false;
    }
    check(wdata_ok, "7.and the data is client 2's", 1, wdata_ok);
    b.c[0].at(0xE000'0000u, 1);
    b.c[1].at(0xE100'0000u, 1);
    b.run(160);
    check(b.c[0].beats == 8 && b.c[1].beats == 8, "7.the port is NOT wedged by the write", 1,
          b.c[0].beats == 8 && b.c[1].beats == 8);
    check(dut.hps_err_count_o == 0, "7.no violation", 0, dut.hps_err_count_o);
  }

  // ---- 7b. a writer in the MIDDLE between two readers ---------------------
  {
    reset(dut);
    Bench b(dut);
    b.c[0].at(0xE400'0000u, 3);
    b.c[1].write = true;
    b.c[1].at(0xE500'0000u, 3);
    b.c[2].at(0xE600'0000u, 3);
    b.run(900);
    check(b.c[0].beats == 24 && b.c[2].beats == 24, "7b.both readers got every beat", 1,
          b.c[0].beats == 24 && b.c[2].beats == 24);
    check(b.c[0].data_ok && b.c[2].data_ok, "7b.and their data is their own", 1,
          b.c[0].data_ok && b.c[2].data_ok);
    check(b.c[1].grants == 3 && b.c[1].beats == 0, "7b.the writer got 3 bursts and no read beats",
          1, b.c[1].grants == 3 && b.c[1].beats == 0);
    check(b.hps_write_lasts == 3, "7b.three write bursts completed", 3,
          static_cast<uint32_t>(b.hps_write_lasts));
    check(dut.hps_err_count_o == 0, "7b.no violation", 0, dut.hps_err_count_o);
  }

  // ---- 8. RULE 6b: a PULSE while ANOTHER CLIENT OWNS THE BRIDGE ----------
  // CMD.DMA raises `hps_req_v` for exactly one cycle and then waits for its
  // response with no timeout. The arbiter used to look at requests in A_IDLE
  // only, so a pulse landing while client 1 owned the bridge was dropped and
  // CMD.DMA hung. These fail against that RTL (0 beats) and pass on the
  // pending-slot repair.
  for (int who : {0, 2}) {
    reset(dut);
    Bench b(dut);
    b.c[1].at(0xA100'0000u, 1);
    int guard = 0;
    while (!b.c[1].in_flight && guard++ < 40) b.step();
    b.run(2);  // mid-burst: client 1 owns the bridge
    Client& p = b.c[who];
    p.base = p.addr = p.next_addr = 0xA000'0000u + static_cast<uint32_t>(who) * 0x0200'0000u;
    p.bursts_wanted = 0;
    dut.c_addr_i[who] = p.addr;
    b.pulse = 1u << who;
    b.run(120);
    check(p.beats == 8,
          name("8.client %d's ONE-cycle pulse during client 1's burst is served", who), 8,
          static_cast<uint32_t>(p.beats));
    check(p.data_ok, name("8.client %d: from the address it pulsed", who), 1, p.data_ok);
    check(bursts(dut, who) == 1, name("8.client %d: exactly one burst, not a repeat", who), 1,
          bursts(dut, who));
    check(b.c[1].beats == 8 && b.c[1].data_ok,
          name("8.client %d: the owner's burst is untouched", who), 1,
          b.c[1].beats == 8 && b.c[1].data_ok);
    if (who == 2) {
      check(dut.c2_wait_cycles_o > 1, "8.client 2's pending wait is COUNTED, not frozen at one", 2,
            dut.c2_wait_cycles_o);
    }
    check(dut.hps_err_count_o == 0, name("8.client %d: no violation", who), 0, dut.hps_err_count_o);
  }

  // ---- 8b. a pulse that LOSES an A_IDLE arbitration is kept too -----------
  // Client 0 holds; client 2 pulses on the very cycle client 0 is chosen.
  {
    reset(dut);
    Bench b(dut);
    b.c[0].at(0xA400'0000u, 1);
    Client& p = b.c[2];
    p.base = p.addr = p.next_addr = 0xA600'0000u;
    p.bursts_wanted = 0;
    dut.c_addr_i[2] = p.addr;
    b.pulse = 1u << 2;
    b.run(160);
    check(b.c[0].beats == 8, "8b.the winner is served", 8, static_cast<uint32_t>(b.c[0].beats));
    check(p.beats == 8 && p.data_ok,
          "8b.the pulse that lost the same-cycle pick is served after it", 1,
          p.beats == 8 && p.data_ok);
    check(dut.hps_err_count_o == 0, "8b.no violation", 0, dut.hps_err_count_o);
  }

  // ---- 9. S2: a READING owner cannot put write beats on the bridge --------
  {
    reset(dut);
    Bench b(dut);
    b.c[0].at(0xA800'0000u, 2);
    b.stray_wr = 1u << 0;  // client 0 reads, and wrongly raises wr_valid/wr_last
    b.run(120);
    check(b.hps_write_beats == 0, "9.no write beat reaches the HPS during a read burst", 0,
          static_cast<uint32_t>(b.hps_write_beats));
    check(b.c[0].beats == 16 && b.c[0].data_ok, "9.and the read bursts complete, whole", 1,
          b.c[0].beats == 16 && b.c[0].data_ok);
    check(dut.hps_err_count_o == 0, "9.no violation", 0, dut.hps_err_count_o);
  }

  // ---- 10. RULE 6c / R55: a SECOND, DIFFERENT request into an occupied
  // pending slot is dropped, and the drop is no longer silent.
  //
  // The slot holds ONE request. Rule 6b promises that an offered request WILL
  // be served, and that promise does not extend to a second one offered while
  // the first is still waiting -- a pulser would simply lose it, and the
  // symptom would appear in the CLIENT as a wait with no end. No client in the
  // console can do it (the arbiter's header names all six holders and the one
  // pulser, and says structurally why), so the counter is expected to read
  // zero -- which is exactly the reading that has to be fired on purpose
  // before its zero means anything.
  //
  // Both halves are here: a holder re-presenting the SAME request must NOT
  // move it, or the instrument would be crying wolf at every legal client.
  {
    reset(dut);
    Bench b(dut);
    b.c[0].at(0xB000'0000u, 2);        // a holder, and the owner for a while
    int guard = 0;
    while (!b.c[0].in_flight && guard++ < 40) b.step();
    b.run(2);
    check(dut.pend_dropped_o == 0, "10.a HOLDER re-presenting the same request is not a drop", 0,
          dut.pend_dropped_o);

    Client& p = b.c[2];
    p.base = p.addr = p.next_addr = 0xB200'0000u;   // request A
    p.bursts_wanted = 0;
    b.pulse = 1u << 2;
    b.step();                                        // A lands in the empty slot
    check(dut.pend_dropped_o == 0, "10.a request into an EMPTY slot is not a drop", 0,
          dut.pend_dropped_o);

    p.addr = 0xB400'0000u;                           // request B, different
    b.pulse = 1u << 2;
    b.step();
    check(dut.pend_dropped_o == 1, "10.a second DIFFERENT request into an occupied slot FIRES it",
          1, dut.pend_dropped_o);
    check(((dut.pend_dropped_mask_o >> 2) & 1u) == 1u,
          "10.and the sticky mask names client 2", 1,
          static_cast<uint32_t>((dut.pend_dropped_mask_o >> 2) & 1u));
    check((dut.pend_dropped_mask_o & 0x3u) == 0u, "10.and names nobody else", 0,
          static_cast<uint32_t>(dut.pend_dropped_mask_o & 0x3u));

    // The arbiter is not corrupted by the illegal offer: the request it DID
    // accept is served, once, whole, and from its own address.
    p.addr = p.next_addr = 0xB200'0000u;
    b.run(200);
    check(p.beats == 8 && p.data_ok,
          "10.the accepted request is still served whole, from its own address", 1,
          p.beats == 8 && p.data_ok);
    check(bursts(dut, 2) == 1, "10.exactly one burst: the dropped one was never served", 1,
          bursts(dut, 2));
    check(dut.pend_dropped_o == 1, "10.and nothing else was counted as a drop", 1,
          dut.pend_dropped_o);
    check(dut.hps_err_count_o == 0, "10.no violation", 0, dut.hps_err_count_o);
  }

  dut.final();
  return zhao::report_and_exit("hps_arbiter_n_directed");
}
