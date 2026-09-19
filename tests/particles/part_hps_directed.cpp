// part_hps_directed.cpp -- the particle generation store, end to end through
// DDR: `tb_part_hps_chain` = the REAL `zhao_part_hps` + `zhao_part_state` +
// `zhao_hps_arbiter_n` + `zhao_hps_bridge`. This file plays PART.UPDATE,
// PART.SPAWN and the HPS (plan D10: in Verilator the harness IS the HPS).
//
// What it proves, each against the contract's own words
// (design/contracts/PART.STATE.md):
//
//   A. THE VALUE TRAVERSES. Records the HPS puts in buffer 0 are offered to
//      UPDATE in order; the verdicts and children PART.STATE writes land in
//      buffer 1 densely ("survivors compacted first ... children appended
//      after"); and the NEXT tick offers exactly buffer 1's contents, read back
//      out of DDR -- so a record crosses DDR twice and is compared bit for bit
//      each time. The next generation's length is the hardware's count.
//   B. AN EMPTY GENERATION FINISHES ITS TICK (`zhao_part_state.rd_empty_i`,
//      added with this block). It used to wait forever for a last record.
//   C. BURSTS, TAILS AND CONTENTION: thirteen records (three full bursts and
//      a one-record tail each way) while a higher-priority rival asks the
//      bridge again shortly after every grant it gets, and the HPS grants late
//      and streams with gaps.
//   D. EVERY COUNTER IS FIRED BY STIMULUS: unseeded, dropped, refused (both
//      reasons), bursts, records -- and the bridge's own `wr_early_beats` and
//      `hps_err_count` read ZERO while its request counter moved, so the zero
//      is a compliant client rather than an idle one.
#include <cstdint>
#include <cstdio>
#include <deque>
#include <map>
#include <vector>

#include "Vtb_part_hps_chain.h"
#include "zhao_sim.hpp"

namespace {

int g_fail = 0;
int g_checks = 0;

void check(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

void check_eq(uint64_t got, uint64_t want, const char* what) {
  ++g_checks;
  if (got != want) {
    ++g_fail;
    std::printf("FAIL: %s: expected 0x%llx, got 0x%llx\n", what,
                static_cast<unsigned long long>(want),
                static_cast<unsigned long long>(got));
  }
}

struct Rec {
  uint64_t lo = 0, hi = 0;
  bool operator==(const Rec& o) const { return lo == o.lo && hi == o.hi; }
};

// species lives at record bits [103:97] = hi bits [39:33] (PART.STATE.md,
// the frozen particle128 layout). Everything else is an arbitrary pattern:
// the streamer carries bits and PART.STATE interprets only `species`.
Rec make_rec(uint32_t id, uint32_t species) {
  Rec r;
  r.lo = 0x9E3779B97F4A7C15ull * (id + 1) ^ 0x0123456789ABCDEFull;
  const uint64_t mask = 0x7Full << 33;
  r.hi = ((0xC2B2AE3D27D4EB4Full * (id + 7)) & ~mask) |
         (static_cast<uint64_t>(species & 3u) << 33);
  return r;
}

constexpr uint32_t kBase0 = 0x00010000u;
constexpr uint32_t kBase1 = 0x00020000u;
constexpr uint32_t kRival = 0x00080000u;
constexpr int kClientEngine1 = 3;   // zhao_pkg::ZHAO_CLIENT_ENGINE1

struct Hps {
  std::map<uint32_t, uint64_t> mem;   // 64-bit words by byte address
  // one burst at a time, which is the bridge's own law
  enum { IDLE, GRANT_WAIT, READ, WRITE } st = IDLE;
  uint32_t addr = 0;
  int beats = 0, done = 0, wait = 0;
  uint32_t lfsr = 0xACE1u;
  bool jitter = false;
  int requests = 0;

  bool rnd() {
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
    return (lfsr & 3u) == 0;
  }
  uint64_t rd(uint32_t a) const {
    auto it = mem.find(a);
    return it == mem.end() ? 0xDEADDEADDEADDEADull : it->second;
  }
  void put(uint32_t base, int idx, const Rec& r) {
    mem[base + 16u * idx] = r.lo;
    mem[base + 16u * idx + 8u] = r.hi;
  }
  Rec get(uint32_t base, int idx) const {
    Rec r;
    r.lo = rd(base + 16u * idx);
    r.hi = rd(base + 16u * idx + 8u);
    return r;
  }
};

struct Bench {
  Vtb_part_hps_chain* v;
  Hps hps;
  // UPDATE's side
  std::deque<Rec> pending;        // offered and not yet judged
  std::vector<Rec> offered;       // every record UPDATE saw, in order
  std::vector<bool> survive_plan; // by offer index within the tick
  uint64_t modify_xor = 0;        // UPDATE's "advance": xor into the low word
  // SPAWN's side
  std::deque<Rec> children;
  // rival
  // A rival that asks CONTINUOUSLY starves every higher index by the arbiter's
  // own law (MEM.HPS.ARBITER rule 7), which the arbiter's tests pin. This one
  // asks again a fixed while after each of its bursts is granted, which is
  // contention rather than starvation.
  bool rival_on = false;
  bool rival_want = false;
  int rival_cool = 0;
  int rival_grants = 0;
  // bridge tag evidence
  int particle_reqs = 0, particle_reqs_engine1 = 0;

  explicit Bench(Vtb_part_hps_chain* d) : v(d) {}

  void drive_hps_inputs() {
    v->hps_req_grant_i = 0;
    v->hps_rd_valid_i = 0;
    v->hps_rd_last_i = 0;
    v->hps_rd_data_i = 0;
    switch (hps.st) {
      case Hps::IDLE:
        break;
      case Hps::GRANT_WAIT:
        if (hps.wait == 0) v->hps_req_grant_i = 1;
        break;
      case Hps::READ:
        if (hps.wait == 0 && !(hps.jitter && hps.rnd())) {
          v->hps_rd_valid_i = 1;
          v->hps_rd_data_i = hps.rd(hps.addr + 8u * hps.done);
          v->hps_rd_last_i = (hps.done == hps.beats - 1);
        }
        break;
      case Hps::WRITE:
        break;
    }
  }

  // Sample the design's outputs BEFORE the edge; commit after.
  void cycle() {
    // ---- UPDATE ----
    v->prt_ready_i = pending.size() < 2 ? 1 : 0;
    v->vrd_valid_i = pending.empty() ? 0 : 1;
    if (!pending.empty()) {
      const Rec& f = pending.front();
      const size_t idx = offered.size() - pending.size();
      const bool surv = idx < survive_plan.size() ? survive_plan[idx] : true;
      v->vrd_survive_i = surv ? 1 : 0;
      v->vrd_record_i[0] = static_cast<uint32_t>(f.lo ^ modify_xor);
      v->vrd_record_i[1] = static_cast<uint32_t>((f.lo ^ modify_xor) >> 32);
      v->vrd_record_i[2] = static_cast<uint32_t>(f.hi);
      v->vrd_record_i[3] = static_cast<uint32_t>(f.hi >> 32);
    }
    // ---- SPAWN ----
    v->chl_valid_i = children.empty() ? 0 : 1;
    v->chl_busy_i = children.empty() ? 0 : 1;
    if (!children.empty()) {
      const Rec& c = children.front();
      v->chl_record_i[0] = static_cast<uint32_t>(c.lo);
      v->chl_record_i[1] = static_cast<uint32_t>(c.lo >> 32);
      v->chl_record_i[2] = static_cast<uint32_t>(c.hi);
      v->chl_record_i[3] = static_cast<uint32_t>(c.hi >> 32);
    }
    // ---- rival ----
    if (rival_on && !rival_want && rival_cool == 0) rival_want = true;
    v->rv_valid_i = rival_want ? 1 : 0;
    v->rv_addr_i = kRival;
    v->rv_len_i = 64;
    // ---- HPS ----
    drive_hps_inputs();

    v->clk = 0;
    v->eval();

    const bool prt_fire = v->prt_valid_o && v->prt_ready_i;
    Rec pr;
    if (prt_fire) {
      pr.lo = static_cast<uint64_t>(v->prt_record_o[0]) |
              (static_cast<uint64_t>(v->prt_record_o[1]) << 32);
      pr.hi = static_cast<uint64_t>(v->prt_record_o[2]) |
              (static_cast<uint64_t>(v->prt_record_o[3]) << 32);
    }
    const bool vrd_fire = v->vrd_valid_i && v->vrd_ready_o;
    const bool chl_fire = v->chl_valid_i && v->chl_ready_o;
    const bool rv_granted = v->rv_grant_o;
    if (rv_granted) ++rival_grants;
    if (v->br_req_valid_o && v->br_req_client_o != 4) {
      ++particle_reqs;
      if (v->br_req_client_o == kClientEngine1) ++particle_reqs_engine1;
    }

    // HPS side, sampled before the edge
    const bool hreq = v->hps_req_valid_o;
    const bool hwr = v->hps_req_write_o;
    const uint32_t haddr = v->hps_req_addr_o;
    const int hlen = v->hps_req_len_o;
    const bool hgrant = v->hps_req_grant_i;
    const bool hrd = v->hps_rd_valid_i;
    const bool hrd_last = v->hps_rd_last_i;
    const bool hwv = v->hps_wr_valid_o;
    const uint64_t hwd = v->hps_wr_data_o;
    const bool hwl = v->hps_wr_last_o;

    v->clk = 1;
    v->eval();

    if (vrd_fire) pending.pop_front();
    if (prt_fire) {
      offered.push_back(pr);
      pending.push_back(pr);
    }
    if (chl_fire) children.pop_front();
    if (rival_cool > 0) --rival_cool;
    if (rv_granted) {
      rival_want = false;
      rival_cool = 24;
    }

    // ---- the HPS state machine ----
    switch (hps.st) {
      case Hps::IDLE:
        if (hreq) {
          hps.st = Hps::GRANT_WAIT;
          hps.addr = haddr;
          hps.beats = (hlen + 7) / 8;
          hps.done = 0;
          hps.wait = hps.jitter ? 3 : 1;
          ++hps.requests;
          // remember direction for after the grant
          hps.lfsr ^= hwr ? 0x10u : 0x20u;
          write_pending_ = hwr;
        }
        break;
      case Hps::GRANT_WAIT:
        if (hps.wait > 0) {
          --hps.wait;
        } else if (hgrant) {
          hps.st = write_pending_ ? Hps::WRITE : Hps::READ;
          hps.wait = write_pending_ ? 0 : (hps.jitter ? 5 : 2);
        }
        break;
      case Hps::READ:
        if (hps.wait > 0) {
          --hps.wait;
        } else if (hrd) {
          ++hps.done;
          if (hrd_last) hps.st = Hps::IDLE;
        }
        break;
      case Hps::WRITE:
        if (hwv) {
          hps.mem[hps.addr + 8u * hps.done] = hwd;
          ++hps.done;
          if (hwl) hps.st = Hps::IDLE;
        }
        break;
    }
  }

  void idle_inputs() {
    v->seed_valid_i = 0;
    v->tick_i = 0;
  }

  void reset() {
    idle_inputs();
    v->rst_n = 0;
    cycle();
    cycle();
    v->rst_n = 1;
    cycle();
  }

  bool seed(uint32_t b0, uint32_t b1, int buf, int count) {
    v->cfg_base0_i = b0;
    v->cfg_base1_i = b1;
    v->seed_buf_i = buf;
    v->seed_count_i = count;
    v->seed_valid_i = 1;
    for (int g = 0; g < 100; ++g) {
      const bool rdy = v->seed_ready_o;
      cycle();
      if (rdy) break;
    }
    v->seed_valid_i = 0;
    return true;
  }

  void pulse_tick() {
    v->tick_i = 1;
    cycle();
    v->tick_i = 0;
  }

  // Run one tick to completion (the streamer's swap, not PART.STATE's done).
  bool run_tick(int max_cycles = 20000) {
    offered.clear();
    pending.clear();
    const uint32_t t0 = v->ticks_o;
    pulse_tick();
    for (int g = 0; g < max_cycles; ++g) {
      if (v->ticks_o != t0 && !v->hps_busy_o) return true;
      cycle();
    }
    return false;
  }

 private:
  bool write_pending_ = false;
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vtb_part_hps_chain;  // heap + exit_hard: see zhao_sim.hpp
  Bench b(top);
  b.reset();

  // ======================================================================
  // D (first half): nothing ticks before a population exists; bad seeds
  // are refused and counted, for both reasons.
  // ======================================================================
  b.pulse_tick();
  for (int i = 0; i < 5; ++i) b.cycle();
  check_eq(top->ticks_unseeded_o, 1, "a tick before any seed is not passed on");
  check_eq(top->ticks_o, 0, "no tick ran unseeded");
  b.seed(kBase0 + 0x10u, kBase1, 0, 3);
  check_eq(top->seeds_refused_o, 1, "a misaligned base refuses the seed");
  b.seed(kBase0, kBase1, 0, 17);
  check_eq(top->seeds_refused_o, 2, "a count above CAPACITY refuses the seed");
  check_eq(top->seeds_o, 0, "neither refused seed was taken");

  // ======================================================================
  // A: the value traverses DDR -> PART.STATE -> DDR -> PART.STATE.
  // ======================================================================
  std::vector<Rec> gen0;
  for (int i = 0; i < 6; ++i) {
    gen0.push_back(make_rec(i, i % 4));
    b.hps.put(kBase0, i, gen0.back());
  }
  b.seed(kBase0, kBase1, 0, 6);
  check_eq(top->seeds_o, 1, "the good seed is taken");
  check_eq(top->cur_count_o, 6, "seeded count");
  check_eq(top->cur_buf_o, 0, "seeded buffer");

  b.survive_plan = {true, true, false, true, true, true};  // record 2 dies
  b.modify_xor = 0x00000000000A0A0Aull;                     // UPDATE advanced it
  const Rec c0 = make_rec(100, 1), c1 = make_rec(101, 2);
  b.children = {c0, c1};
  check(b.run_tick(), "tick 1 completes");

  check_eq(b.offered.size(), 6, "tick 1 offered every seeded record");
  for (size_t i = 0; i < b.offered.size() && i < gen0.size(); ++i)
    check(b.offered[i] == gen0[i], "tick 1 offered the DDR record bit for bit, in order");

  std::vector<Rec> want1;
  for (int i = 0; i < 6; ++i)
    if (i != 2) want1.push_back(Rec{gen0[i].lo ^ b.modify_xor, gen0[i].hi});
  want1.push_back(c0);
  want1.push_back(c1);
  check_eq(top->cur_buf_o, 1, "tick 1 swapped to buffer 1");
  check_eq(top->cur_count_o, want1.size(), "tick 1's count is what was written");
  for (size_t i = 0; i < want1.size(); ++i)
    check(b.hps.get(kBase1, static_cast<int>(i)) == want1[i],
          "buffer 1 holds survivors compacted then children, bit for bit");
  // the read buffer was never written within the tick
  for (int i = 0; i < 6; ++i)
    check(b.hps.get(kBase0, i) == gen0[i], "tick 1 never wrote its read buffer");

  // Tick 2 reads buffer 1 BACK OUT OF DDR.
  b.survive_plan.clear();
  b.modify_xor = 0;
  check(b.run_tick(), "tick 2 completes");
  check_eq(b.offered.size(), want1.size(), "tick 2 offered the whole of generation 1");
  for (size_t i = 0; i < b.offered.size() && i < want1.size(); ++i)
    check(b.offered[i] == want1[i], "tick 2 offered what tick 1 wrote, read back from DDR");
  check_eq(top->cur_buf_o, 0, "tick 2 swapped back to buffer 0");
  for (size_t i = 0; i < want1.size(); ++i)
    check(b.hps.get(kBase0, static_cast<int>(i)) == want1[i],
          "an untouched survivor is written back bit-identical");
  check_eq(top->records_read_o, 6 + 7, "records read over two ticks");
  check_eq(top->records_written_o, 7 + 7, "records written over two ticks");

  // D (second half): a tick while one runs is dropped and counted.
  b.children = {make_rec(200, 3)};
  b.pulse_tick();
  b.pulse_tick();                           // arrives while tick 3 runs
  check_eq(top->ticks_dropped_o, 1, "a tick during a tick is counted as dropped");
  for (int g = 0; g < 20000 && top->hps_busy_o; ++g) b.cycle();
  check_eq(top->cur_count_o, 8, "tick 3 appended its child");

  // ======================================================================
  // B: an empty generation finishes its tick.
  // ======================================================================
  b.seed(kBase0, kBase1, 1, 0);
  const uint32_t tb = top->ticks_o;
  check(b.run_tick(), "an empty generation's tick completes");
  check_eq(top->ticks_o, tb + 1, "the empty tick is counted");
  check_eq(b.offered.size(), 0, "an empty generation offers nothing");
  check_eq(top->cur_count_o, 0, "an empty generation stays empty");
  check_eq(top->cur_buf_o, 0, "and still swaps");

  // ======================================================================
  // C: bursts, tails, contention, late grants and gaps.
  // ======================================================================
  std::vector<Rec> big;
  for (int i = 0; i < 13; ++i) {
    big.push_back(make_rec(300 + i, i % 4));
    b.hps.put(kBase1, i, big.back());
  }
  b.seed(kBase0, kBase1, 1, 13);
  b.hps.jitter = true;
  b.rival_on = true;
  b.modify_xor = 0x5500ull;
  const uint32_t rb0 = top->rd_bursts_o, wb0 = top->wr_bursts_o;
  const uint32_t wait0 = top->arb_c1_wait_o;
  const int rg0 = b.rival_grants;
  check(b.run_tick(200000), "a thirteen-record tick completes under contention");
  b.rival_on = false;
  for (int g = 0; g < 200 && top->rv_valid_i; ++g) b.cycle();   // let its last ask land
  check_eq(b.offered.size(), 13, "all thirteen offered");
  for (size_t i = 0; i < b.offered.size() && i < big.size(); ++i)
    check(b.offered[i] == big[i], "contended read is bit-exact and in order");
  for (int i = 0; i < 13; ++i)
    check(b.hps.get(kBase0, i) == Rec{big[i].lo ^ 0x5500ull, big[i].hi},
          "contended write is bit-exact and dense");
  check_eq(top->rd_bursts_o - rb0, 4, "13 records read as 4+4+4+1");
  check_eq(top->wr_bursts_o - wb0, 4, "13 records written as 4+4+4+1");
  check(top->arb_c1_wait_o > wait0, "the rival made the store wait (arbiter counter)");
  check(b.rival_grants > rg0, "the rival was served during the tick");

  // The bridge's view of this client.
  check_eq(top->br_wr_early_o, 0, "no write beat offered before the bridge could take it");
  check_eq(top->br_err_count_o, 0, "no malformed or colliding burst");
  check(b.hps.requests > 20, "the bridge carried the traffic");
  check(b.particle_reqs > 0 && b.particle_reqs == b.particle_reqs_engine1,
        "every particle burst carries the composer's client tag");

  std::printf("part_hps_directed: %d checks, %d failed\n", g_checks, g_fail);
  zhao::exit_hard(g_fail == 0 ? 0 : 1);
}
