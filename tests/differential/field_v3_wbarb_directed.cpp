// field_v3_wbarb_directed.cpp — the writeback arbiter: merge several streams
// onto the register file's ONE write port, under three policies, and count
// each claimant separately so starvation is visible rather than inferred.
//
// THE ORACLE IS THE CONTRACT, as it is for the dispatcher. This block computes
// no value; it decides who gets a port. zfield has nothing to say about that,
// and inventing a reference model of an arbiter would only prove that two
// arbiters written by the same hand agree.
//
// THE PROPERTIES:
//
//   1. THE WINNER'S DATA REACHES THE PORT, never a loser's. Each claimant
//      carries a distinguishable context, register and value, so a crossed
//      mux is visible in the value itself rather than only in a count.
//   2. READY IS ONE-HOT. Two claimants told they were served on the same
//      clock is two writes into a port that takes one.
//   3. A LONE CLAIMANT WINS EVERY CLOCK, AND STALLS ZERO TIMES. This is the
//      case the multiplier bank's M08 mutant survived without: a counter that
//      counts REQUESTS reads identically to one that counts LOSSES in any
//      test where the claimant loses every clock, and a priority test has
//      exactly that shape. The distinguishing case is asking and WINNING.
//   4. EACH FIXED POLICY PICKS THE CLAIMANT IT CLAIMS TO. Policy 0 the
//      lowest, policy 1 the highest.
//   5. ROUND ROBIN STARVES NOBODY. Every claimant is served, and no claimant
//      takes more than one more turn than any other over a run.
//   6. NOBODY ASKING MEANS NO WRITE. wr_en_o must be low, because the
//      register file writes on it.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_v3_wbarb.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

constexpr int kClaimants = 3;

constexpr int POL_LOW_FIRST = 0;
constexpr int POL_HIGH_FIRST = 1;
constexpr int POL_ROUND_ROBIN = 2;

/** Values that say which claimant they came from, in every field. */
void drive(Vzhao_field_v3_wbarb& t, uint8_t asking) {
  t.req_valid_i = asking;
  for (int c = 0; c < kClaimants; ++c) {
    t.req_ctx_i[c] = (uint8_t)(c + 1);           // 1, 2, 3
    t.req_reg_i[c] = (uint8_t)(8 * (c + 1));     // 8, 16, 24
    t.req_data_i[c] = (uint32_t)(0xC0000000u | (uint32_t)c);
  }
}

struct Counts {
  int served[kClaimants] = {0, 0, 0};
  int writes = 0;
  int crossed = 0;    // the port carried a field belonging to a different claimant
  int multi = 0;      // more than one claimant told it was served
  int ready_idle = 0; // somebody was told it was served with no write happening
  // WHO WON, IN ORDER. Counts and fairness both pass under a rotation that is
  // merely phase-shifted (W07: starting one claimant late is still 10/10/10
  // over 30 clocks). The sequence is the only thing that separates them.
  std::vector<int> order;
};

/** Run `clocks` cycles with a fixed asking set and policy, recording what happened. */
Counts run(Vzhao_field_v3_wbarb& t, uint8_t asking, int policy, int clocks,
           bool do_reset = true) {
  Counts n;
  // `do_reset = false` continues from whatever state the last run left. That
  // is not a convenience: the POLICY IS A RUNTIME INPUT, so changing it
  // without a reset is the case it exists for, and W06 -- rotating the round
  // robin pointer under every policy -- is invisible to any test that resets
  // in between.
  if (do_reset) {
    t.rst_n = 0;
    t.policy_i = (uint8_t)policy;
    drive(t, 0);
    t.eval();
    for (int i = 0; i < 4; ++i) zhao::tick(t);
    t.rst_n = 1;
  }
  t.policy_i = (uint8_t)policy;
  drive(t, asking);
  t.eval();

  for (int k = 0; k < clocks; ++k) {
    t.eval();
    int winners = 0, who = -1;
    for (int c = 0; c < kClaimants; ++c) {
      if ((t.req_ready_o >> c) & 1) {
        ++winners;
        who = c;
      }
    }
    if (winners > 1) ++n.multi;
    // READY WITHOUT A REQUEST IS ITS OWN FAULT. Everything else in this loop
    // is guarded by wr_en_o, so a claimant told it was served while nobody
    // asked would be recorded nowhere. Dropping the `any_c` term from
    // req_ready_o is exactly that defect, and it would have survived a suite
    // that only looks at the port.
    if (winners > 0 && !t.wr_en_o) ++n.ready_idle;
    if (t.wr_en_o) {
      ++n.writes;
      if (who >= 0) {
        ++n.served[who];
        n.order.push_back(who);
        // Law 1: every field on the port must belong to the winner.
        if ((int)t.wr_ctx_o != who + 1 || (int)t.wr_reg_o != 8 * (who + 1) ||
            (uint32_t)t.wr_data_o != (0xC0000000u | (uint32_t)who)) {
          ++n.crossed;
        }
      }
    }
    zhao::tick(t);
  }
  return n;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_field_v3_wbarb t;

  printf("== section 1: nobody asking means no write ==\n");
  {
    const Counts n = run(t, 0x0, POL_HIGH_FIRST, 32);
    check(n.writes == 0, "wr_en_o stays low when nobody asks", 0, (uint32_t)n.writes);
    check(n.ready_idle == 0, "and NOBODY is told it was served", 0, (uint32_t)n.ready_idle);
    check(t.req_ready_o == 0, "req_ready_o is entirely zero with no requests", 0,
          (int)t.req_ready_o);
    for (int c = 0; c < kClaimants; ++c) {
      check((int)t.served_o[c] == 0,
            ("claimant " + std::to_string(c) + " was served nothing").c_str(), 0,
            (int)t.served_o[c]);
      check((int)t.stalled_o[c] == 0,
            ("claimant " + std::to_string(c) + " stalled nothing -- it never asked").c_str(),
            0, (int)t.stalled_o[c]);
    }
  }

  printf("== section 2: a LONE claimant wins every clock and stalls ZERO times ==\n");
  {
    // Law 3, and the reason it is its own section: this is the only shape in
    // which "counted requests" and "counted losses" give different numbers.
    for (int c = 0; c < kClaimants; ++c) {
      const int clocks = 24;
      const Counts n = run(t, (uint8_t)(1u << c), POL_HIGH_FIRST, clocks);
      const std::string w = "lone claimant " + std::to_string(c);
      check(n.served[c] == clocks, (w + ": served every clock").c_str(), (uint32_t)clocks,
            (uint32_t)n.served[c]);
      check(n.crossed == 0, (w + ": the port carried its own fields").c_str(), 0,
            (uint32_t)n.crossed);
      check((int)t.stalled_o[c] == 0,
            (w + ": stalled_o is ZERO -- it counts losses, not requests").c_str(), 0,
            (int)t.stalled_o[c]);
      check((int)t.served_o[c] == clocks, (w + ": served_o agrees").c_str(), (uint32_t)clocks,
            (int)t.served_o[c]);
    }
  }

  printf("== section 3: policy 1 -- the HIGHEST claimant takes everything ==\n");
  {
    const int clocks = 24;
    const Counts n = run(t, 0x7, POL_HIGH_FIRST, clocks);
    check(n.served[2] == clocks, "claimant 2 is served every clock", (uint32_t)clocks,
          (uint32_t)n.served[2]);
    check(n.served[1] == 0, "claimant 1 gets nothing", 0, (uint32_t)n.served[1]);
    check(n.served[0] == 0, "claimant 0 gets nothing", 0, (uint32_t)n.served[0]);
    check(n.multi == 0, "ready is one-hot throughout", 0, (uint32_t)n.multi);
    check(n.ready_idle == 0, "and ready never fires without a write", 0,
          (uint32_t)n.ready_idle);
    check(n.crossed == 0, "and the port always carried the winner's fields", 0,
          (uint32_t)n.crossed);
    // The starvation is REPORTED, not merely suffered. This is the number the
    // composed engine's measurement will read.
    check((int)t.stalled_o[0] == clocks, "claimant 0's starvation is counted",
          (uint32_t)clocks, (int)t.stalled_o[0]);
    check((int)t.stalled_o[1] == clocks, "and claimant 1's", (uint32_t)clocks,
          (int)t.stalled_o[1]);
    // W14: served_o counting REQUESTS instead of GRANTS is invisible for a
    // claimant that wins every clock, which is the only case section 2 has.
    // The separating case is a claimant that ASKS AND LOSES -- it is right
    // here, and it was unasserted.
    check((int)t.served_o[0] == 0, "claimant 0 was SERVED nothing, though it asked", 0,
          (int)t.served_o[0]);
    check((int)t.served_o[1] == 0, "and claimant 1 likewise", 0, (int)t.served_o[1]);
    check((int)t.served_o[2] == clocks, "while claimant 2 was served every clock",
          (uint32_t)clocks, (int)t.served_o[2]);
    printf("   MEASURED policy 1: served %d/%d/%d, stalled %u/%u/%u\n", n.served[0], n.served[1],
           n.served[2], t.stalled_o[0], t.stalled_o[1], t.stalled_o[2]);
  }

  printf("== section 4: policy 0 -- the LOWEST claimant takes everything ==\n");
  {
    const int clocks = 24;
    const Counts n = run(t, 0x7, POL_LOW_FIRST, clocks);
    check(n.served[0] == clocks, "claimant 0 is served every clock", (uint32_t)clocks,
          (uint32_t)n.served[0]);
    check(n.served[1] == 0, "claimant 1 gets nothing", 0, (uint32_t)n.served[1]);
    check(n.served[2] == 0, "claimant 2 gets nothing", 0, (uint32_t)n.served[2]);
    check(n.crossed == 0, "the port always carried the winner's fields", 0,
          (uint32_t)n.crossed);
    printf("   MEASURED policy 0: served %d/%d/%d\n", n.served[0], n.served[1], n.served[2]);
  }

  printf("== section 5: round robin starves nobody ==\n");
  {
    const int clocks = 30;  // divisible by 3, so a fair share is exact
    const Counts n = run(t, 0x7, POL_ROUND_ROBIN, clocks);
    int lo = n.served[0], hi = n.served[0];
    for (int c = 1; c < kClaimants; ++c) {
      if (n.served[c] < lo) lo = n.served[c];
      if (n.served[c] > hi) hi = n.served[c];
    }
    printf("   MEASURED round robin: served %d/%d/%d over %d clocks\n", n.served[0], n.served[1],
           n.served[2], clocks);
    check(lo > 0, "every claimant is served at least once", 1, lo > 0 ? 1 : 0);
    check(hi - lo <= 1, "and no claimant takes more than one extra turn", 1,
          (hi - lo <= 1) ? 1 : 0);
    check(n.writes == clocks, "the port is busy every clock -- rotation costs nothing",
          (uint32_t)clocks, (uint32_t)n.writes);
    check(n.multi == 0, "ready is one-hot throughout", 0, (uint32_t)n.multi);
    check(n.crossed == 0, "and the port always carried the winner's fields", 0,
          (uint32_t)n.crossed);
  }

  printf("== section 6: two claimants, each policy picks the right one ==\n");
  {
    // Pairs rather than the full set, so a policy that happens to be right
    // about the extremes but wrong in the middle is still caught.
    struct Case {
      uint8_t asking;
      int policy;
      int winner;
      const char* what;
    };
    const Case cases[] = {
        {0x3, POL_HIGH_FIRST, 1, "0 and 1, highest first -> 1"},
        {0x3, POL_LOW_FIRST, 0, "0 and 1, lowest first -> 0"},
        {0x5, POL_HIGH_FIRST, 2, "0 and 2, highest first -> 2"},
        {0x5, POL_LOW_FIRST, 0, "0 and 2, lowest first -> 0"},
        {0x6, POL_HIGH_FIRST, 2, "1 and 2, highest first -> 2"},
        {0x6, POL_LOW_FIRST, 1, "1 and 2, lowest first -> 1"},
    };
    for (const Case& c : cases) {
      const int clocks = 12;
      const Counts n = run(t, c.asking, c.policy, clocks);
      check(n.served[c.winner] == clocks, c.what, (uint32_t)clocks,
            (uint32_t)n.served[c.winner]);
      check(n.crossed == 0, (std::string(c.what) + ": fields intact").c_str(), 0,
            (uint32_t)n.crossed);
    }
  }

  printf("== section 7: round robin's ORDER, not just its totals ==\n");
  {
    // W07 -- starting one claimant late -- is still 10/10/10 over 30 clocks,
    // so counts and fairness both pass. The sequence is what separates them.
    const Counts n = run(t, 0x7, POL_ROUND_ROBIN, 9);
    std::string got;
    for (size_t i = 0; i < n.order.size(); ++i) got += std::to_string(n.order[i]);
    printf("   MEASURED grant order: %s\n", got.c_str());
    check(got == "012012012", "the rotation runs 0,1,2 from reset and repeats",
          0, (uint32_t)(got == "012012012" ? 0 : 1));
  }

  printf("== section 8: the rotation pointer does NOT move under a fixed policy ==\n");
  {
    // W06 -- advancing rr_r whatever the policy -- is invisible to any test
    // that resets between sections, because only the round-robin arm READS it.
    // But the policy is a RUNTIME input: changing it without a reset is the
    // case it exists for.
    //
    // Policy 0 is the one that exposes it. Its winner is always claimant 0, so
    // a rotation that advanced would leave the pointer at 1, and the first
    // round-robin grant would go to claimant 1 instead of 0. (Under policy 1
    // the winner is always 2 and the pointer wraps back to 0, which hides the
    // defect -- so the choice of policy here is the whole test.)
    run(t, 0x7, POL_LOW_FIRST, 12);
    const Counts n = run(t, 0x7, POL_ROUND_ROBIN, 9, /*do_reset=*/false);
    std::string got;
    for (size_t i = 0; i < n.order.size(); ++i) got += std::to_string(n.order[i]);
    printf("   MEASURED order after a policy-0 phase: %s\n", got.c_str());
    check(got == "012012012",
          "the rotation still starts at 0 -- a fixed policy left it alone", 0,
          (uint32_t)(got == "012012012" ? 0 : 1));
  }

  return zhao::report_and_exit("field_v3_wbarb_directed");
}
