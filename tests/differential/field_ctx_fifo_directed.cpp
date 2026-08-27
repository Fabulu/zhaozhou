// field_ctx_fifo_directed.cpp — the Field v3 ready-context FIFO scheduler
// probe (reports/Fieldv3.md Phase 3, probe 1).
//
// A scheduler's oracle is its INVARIANTS, not an arithmetic reference:
//   1. NO INSTRUCTION IS LOST OR DUPLICATED: every started context issues
//      pc 0,1,...,len-1 exactly once, in order, then releases exactly once.
//   2. ONE INSTRUCTION IN FLIGHT PER CONTEXT: after a long op issues, the
//      same context does not issue again before the service latency has
//      elapsed; after ANY issue it does not issue on the next cycle (the
//      requeue trip exists by construction).
//   3. NO CONTEXT IS LOST: everything started eventually completes.
//   4. THE RATE: with eight all-short contexts resident, issue sustains
//      1 instruction/clock over a measured steady window — the whole point
//      of replacing the scan with a FIFO.
//   5. inflight_o is a subset of busy_o every audited cycle.
//
// These are the Fieldv3 formal properties, tested dynamically here; the
// bounded proofs come with the engine, not the probe.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_probe_ctx_fifo.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

constexpr int CTX = 8;

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    return (uint32_t)(s >> 33);
  }
  uint32_t below(uint32_t n) { return n ? next() % n : 0; }
};

struct Plan {
  int len = 0;
  bool is_long[32] = {};
  int lat[32] = {};
};

void load_plan(Vzhao_probe_ctx_fifo& dut, int ctx, const Plan& p) {
  for (int pc = 0; pc < p.len; ++pc) {
    dut.plan_we_i = 1;
    dut.plan_waddr_i = (uint16_t)((ctx << 5) | pc);
    dut.plan_wdata_i = (uint8_t)((p.is_long[pc] ? 0x20 : 0) | (p.lat[pc] & 0x1F));
    zhao::tick(dut);
  }
  dut.plan_we_i = 0;
  dut.eval();
}

void start_ctx(Vzhao_probe_ctx_fifo& dut, int ctx, int len) {
  dut.start_i = 1;
  dut.start_ctx_i = (uint8_t)ctx;
  dut.start_len_i = (uint8_t)len;
  dut.eval();
  int guard = 0;
  while (!dut.start_ready_o && guard++ < 64) {
    zhao::tick(dut);
    dut.eval();
  }
  zhao::tick(dut);
  dut.start_i = 0;
  dut.eval();
}

// Run until all started contexts release; audit every cycle.
// Returns cycles consumed.
struct Audit {
  int issues = 0;
  int dup_or_ooo = 0;   // out-of-order / duplicated pc
  int early_issue = 0;  // issue while owing a service wait or on the requeue trip
  int subset_violation = 0;
  int issue_not_inflight = 0;  // an issuing context MUST be marked in flight
  int done[CTX] = {};
  int next_pc[CTX] = {};
};

// Storm driver: performs the STARTS INSIDE the audited loop (a context that
// is already running issues while its siblings are still being started, and
// an audit that begins after the last start miscounts those issues — a
// harness artifact the first version of this file had), then runs to
// completion under the full audit.
int run_storm(Vzhao_probe_ctx_fifo& dut, const Plan plan[CTX], const bool started[CTX], Audit& a,
              int max_cycles) {
  int earliest_next[CTX];
  for (int c = 0; c < CTX; ++c) earliest_next[c] = 0;
  int pending = -1;  // context currently being offered
  int next_to_start = 0;
  int cycle = 0;
  auto all_done = [&]() {
    for (int c = 0; c < CTX; ++c) {
      if (started[c] && a.done[c] == 0) return false;
    }
    return true;
  };
  while (!all_done() && cycle < max_cycles) {
    // offer the next pending start, one at a time
    if (pending < 0) {
      while (next_to_start < CTX && !started[next_to_start]) ++next_to_start;
      if (next_to_start < CTX) {
        pending = next_to_start;
        dut.start_i = 1;
        dut.start_ctx_i = (uint8_t)pending;
        dut.start_len_i = (uint8_t)plan[pending].len;
        ++next_to_start;
      } else {
        dut.start_i = 0;
      }
    }
    dut.eval();
    const bool start_fires = dut.start_i && dut.start_ready_o;
    if (dut.issue_valid_o) {
      const int c = dut.issue_ctx_o;
      const int pc = dut.issue_pc_o;
      ++a.issues;
      if (((dut.inflight_o >> c) & 1) == 0) ++a.issue_not_inflight;
      if (pc != a.next_pc[c]) ++a.dup_or_ooo;
      a.next_pc[c] = pc + 1;
      if (cycle < earliest_next[c]) ++a.early_issue;
      if (pc + 1 < plan[c].len && plan[c].is_long[pc]) {
        earliest_next[c] = cycle + plan[c].lat[pc] + 2;
      } else {
        earliest_next[c] = cycle + 2;
      }
    }
    if (dut.done_valid_o) ++a.done[dut.done_ctx_o];
    if ((dut.inflight_o & ~dut.busy_o) != 0) ++a.subset_violation;
    zhao::tick(dut);
    ++cycle;
    if (start_fires) pending = -1;
  }
  dut.start_i = 0;
  dut.eval();
  return cycle;
}

int run_all(Vzhao_probe_ctx_fifo& dut, const Plan plan[CTX], const bool started[CTX], Audit& a,
            int max_cycles) {
  int earliest_next[CTX];
  for (int c = 0; c < CTX; ++c) earliest_next[c] = 0;
  int cycle = 0;
  auto all_done = [&]() {
    for (int c = 0; c < CTX; ++c) {
      if (started[c] && a.done[c] == 0) return false;
    }
    return true;
  };
  while (!all_done() && cycle < max_cycles) {
    dut.eval();
    if (dut.issue_valid_o) {
      const int c = dut.issue_ctx_o;
      const int pc = dut.issue_pc_o;
      ++a.issues;
      if (((dut.inflight_o >> c) & 1) == 0) ++a.issue_not_inflight;
      if (pc != a.next_pc[c]) ++a.dup_or_ooo;
      a.next_pc[c] = pc + 1;
      if (cycle < earliest_next[c]) ++a.early_issue;
      // one-in-flight: the requeue trip is >= 2 cycles; a long op adds its
      // service latency before the context can even re-enter the queue.
      if (pc + 1 < plan[c].len && plan[c].is_long[pc]) {
        earliest_next[c] = cycle + plan[c].lat[pc] + 2;
      } else {
        earliest_next[c] = cycle + 2;
      }
    }
    if (dut.done_valid_o) ++a.done[dut.done_ctx_o];
    if ((dut.inflight_o & ~dut.busy_o) != 0) ++a.subset_violation;
    zhao::tick(dut);
    ++cycle;
  }
  return cycle;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  int random_n = 0;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--random") && i + 1 < argc) random_n = atoi(argv[i + 1]);
  }

  Vzhao_probe_ctx_fifo dut;
  // local reset: the shared zhao::reset assumes the byte-stream harness ports
  dut.rst_n = 0;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);
  Prng rng(random_n ? 0xC7F + random_n : 0xC7F);

  if (random_n == 0) {
    printf("== section 1: one context, all short ==\n");
    {
      Plan p;
      p.len = 9;  // 8 ops + END slot
      Plan plans[CTX] = {};
      plans[3] = p;
      bool started[CTX] = {};
      started[3] = true;
      load_plan(dut, 3, p);
      Audit a;
      start_ctx(dut, 3, p.len);
      const int cyc = run_all(dut, plans, started, a, 2000);
      check(a.done[3] == 1, "context released exactly once", 1, a.done[3]);
      check(a.issues == p.len, "every instruction issued exactly once", p.len, a.issues);
      check(a.dup_or_ooo == 0, "pcs in order, none duplicated", 0, a.dup_or_ooo);
      check(a.early_issue == 0, "one instruction in flight per context", 0, a.early_issue);
      check(a.subset_violation == 0, "inflight is a subset of busy", 0, a.subset_violation);
      check(a.issue_not_inflight == 0, "an issuing context is marked in flight", 0,
            a.issue_not_inflight);
      check(dut.busy_o == 0, "no context stays busy", 0, dut.busy_o);
      printf("   (%d cycles for %d issues — the 2-cycle single-context loop)\n", cyc, a.issues);
    }

    printf("== section 2: one context, long ops ==\n");
    {
      Plan p;
      p.len = 6;
      p.is_long[1] = true;
      p.lat[1] = 17;
      p.is_long[3] = true;
      p.lat[3] = 5;
      Plan plans[CTX] = {};
      plans[5] = p;
      bool started[CTX] = {};
      started[5] = true;
      load_plan(dut, 5, p);
      Audit a;
      start_ctx(dut, 5, p.len);
      run_all(dut, plans, started, a, 2000);
      check(a.done[5] == 1, "long-op context released once", 1, a.done[5]);
      check(a.issues == p.len, "long-op context issued its whole plan", p.len, a.issues);
      check(a.dup_or_ooo == 0, "long-op pcs in order", 0, a.dup_or_ooo);
      check(a.early_issue == 0, "service latency respected before re-issue", 0, a.early_issue);
    }

    printf("== section 3: eight resident all-short contexts — THE RATE ==\n");
    {
      Plan plans[CTX];
      bool started[CTX];
      for (int c = 0; c < CTX; ++c) {
        plans[c].len = 31;
        started[c] = true;
        load_plan(dut, c, plans[c]);
      }
      for (int c = 0; c < CTX; ++c) start_ctx(dut, c, plans[c].len);
      // steady window: count issue slots over 64 cycles mid-run
      int window_issues = 0;
      for (int i = 0; i < 64; ++i) {
        dut.eval();
        if (dut.issue_valid_o) ++window_issues;
        zhao::tick(dut);
      }
      printf("   MEASURED issue slots in a 64-cycle steady window: %d\n", window_issues);
      check(window_issues == 64, "THE RATE: 1 instruction/clock with 8 resident contexts", 64,
            window_issues);
      // then drain and account
      Audit a;
      // window already consumed some issues; account the remainder loosely:
      run_all(dut, plans, started, a, 4000);
      for (int c = 0; c < CTX; ++c) {
        check(a.done[c] == 1, "context drains exactly once", 1, a.done[c]);
      }
      check(a.subset_violation == 0, "inflight subset holds through the storm", 0,
            a.subset_violation);
      check(a.issue_not_inflight == 0, "issue implies inflight through the storm", 0,
            a.issue_not_inflight);
    }

    printf("== section 4: mixed long/short storm, full accounting ==\n");
    {
      Plan plans[CTX];
      bool started[CTX];
      for (int c = 0; c < CTX; ++c) {
        plans[c].len = 4 + (int)rng.below(27);
        for (int pc = 0; pc < plans[c].len; ++pc) {
          plans[c].is_long[pc] = rng.below(3) == 0;
          plans[c].lat[pc] = (int)rng.below(20);
        }
        started[c] = true;
        load_plan(dut, c, plans[c]);
      }
      Audit a;
      run_storm(dut, plans, started, a, 20000);
      int want_issues = 0;
      for (int c = 0; c < CTX; ++c) want_issues += plans[c].len;
      check(a.issues == want_issues, "storm: every instruction of every context issued",
            want_issues, a.issues);
      check(a.dup_or_ooo == 0, "storm: order per context intact", 0, a.dup_or_ooo);
      check(a.early_issue == 0, "storm: one-in-flight law intact", 0, a.early_issue);
      for (int c = 0; c < CTX; ++c) {
        check(a.done[c] == 1, "storm: no context lost or double-released", 1, a.done[c]);
      }
      check(dut.busy_o == 0, "storm: all contexts free at the end", 0, dut.busy_o);
    }

    printf("== section 5: a BUSY context cannot be restarted ==\n");
    {
      // A start offered for a running context must be REFUSED (start_ready_o
      // low) for as long as it is busy — accepting it would reset the pc of
      // a context that still owes issues.
      Plan p;
      p.len = 20;
      p.is_long[2] = true;
      p.lat[2] = 25;
      Plan plans[CTX] = {};
      plans[2] = p;
      bool started[CTX] = {};
      started[2] = true;
      load_plan(dut, 2, p);
      Audit a;
      start_ctx(dut, 2, p.len);
      // offer a restart every cycle while it runs; it must never be accepted
      int accepted_while_busy = 0;
      dut.start_i = 1;
      dut.start_ctx_i = 2;
      dut.start_len_i = 3;
      for (int i = 0; i < 200; ++i) {
        dut.eval();
        if (dut.issue_valid_o) {
          const int c = dut.issue_ctx_o;
          ++a.issues;
          if (dut.issue_pc_o != a.next_pc[c]) ++a.dup_or_ooo;
          a.next_pc[c] = dut.issue_pc_o + 1;
        }
        if (dut.done_valid_o) ++a.done[dut.done_ctx_o];
        if (((dut.busy_o >> 2) & 1) && dut.start_ready_o) ++accepted_while_busy;
        // stop offering the moment the context legally releases — a start
        // offered to a FREE context would (correctly) be accepted.
        if (a.done[2] != 0) break;
        zhao::tick(dut);
      }
      dut.start_i = 0;
      dut.eval();
      zhao::tick(dut);
      dut.eval();
      check(accepted_while_busy == 0, "restart of a busy context is refused", 0,
            accepted_while_busy);
      check(a.issues == p.len, "refused restarts never corrupted the pc walk", p.len, a.issues);
      check(a.dup_or_ooo == 0, "pc order intact through the refusal storm", 0, a.dup_or_ooo);
      check(a.done[2] == 1, "the context still released exactly once", 1, a.done[2]);
      check(dut.busy_o == 0, "nothing left busy after the refusal storm", 0, dut.busy_o);
    }
  } else {
    printf("== random storms: %d ==\n", random_n);
    for (int i = 0; i < random_n; ++i) {
      Plan plans[CTX];
      bool started[CTX];
      for (int c = 0; c < CTX; ++c) {
        started[c] = rng.below(4) != 0;  // three quarters of contexts run
        plans[c].len = started[c] ? (1 + (int)rng.below(31)) : 0;
        for (int pc = 0; pc < plans[c].len; ++pc) {
          plans[c].is_long[pc] = rng.below(2) == 0;
          plans[c].lat[pc] = (int)rng.below(32);
        }
        if (started[c]) load_plan(dut, c, plans[c]);
      }
      Audit a;
      run_storm(dut, plans, started, a, 40000);
      int want_issues = 0, want_done = 0;
      for (int c = 0; c < CTX; ++c) {
        if (started[c]) {
          want_issues += plans[c].len;
          ++want_done;
        }
      }
      int got_done = 0;
      for (int c = 0; c < CTX; ++c) got_done += a.done[c];
      char msg[64];
      snprintf(msg, sizeof msg, "storm %d: issues exact", i);
      check(a.issues == want_issues, msg, want_issues, a.issues);
      snprintf(msg, sizeof msg, "storm %d: completions exact", i);
      check(got_done == want_done, msg, want_done, got_done);
      snprintf(msg, sizeof msg, "storm %d: order/in-flight/subset clean", i);
      check(a.dup_or_ooo + a.early_issue + a.subset_violation + a.issue_not_inflight == 0, msg, 0,
            a.dup_or_ooo + a.early_issue + a.subset_violation + a.issue_not_inflight);
    }
  }

  dut.final();
  return zhao::report_and_exit(random_n ? "field_ctx_fifo_random" : "field_ctx_fifo_directed");
}
