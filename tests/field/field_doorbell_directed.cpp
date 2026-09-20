// field_doorbell_directed.cpp -- THE FIELD PROGRAM DOORBELL: does SW.STREAM's
// mailbox reach the loader and the directory, answer every post, and refuse a
// commit that would promise a slot nobody wrote a header for?
//
// The block is `zhao_field_doorbell`, entry I42's closure, owner ruling R43.
// This file drives it STANDALONE: the loader's ready and both directory phases
// are played here, so a case can construct a response the composed engine would
// never produce on demand (an eviction, a hit, a stall). What the composed
// article does is checked separately by the console smoke bench.
//
// EVERY CASE IS A COUNTER THAT MUST MOVE, and the assertions are on the CORRECT
// behaviour rather than on a defect, so nothing here passes only while a bug
// exists (CLAUDE.md: "do not write a test that asserts the bug").
//
//   1  a LOAD WORD reaches the loader verbatim                 load_words_o
//   2  a LOOKUP reaches the directory and its answer returns    lookups_o
//   3  a COMMIT after a HEADER reaches the directory            commits_o
//   4  a COMMIT with NO header is REFUSED, answered, counted    commits_refused_o
//   5  a post into a full mailbox is HELD, never dropped        post_stalls_o
//   6  an insert spends the header, so the next commit needs    commits_refused_o
//      its own
//
// `ret_overflow_o` is the seventh and it CANNOT be fired with legal stimulus:
// the return credit is what makes the state unreachable. That is the committed
// mutant's job -- tests/mutants/zhao_field_doorbell_mutant.sv -- and case 7
// here asserts only that it stays zero, which is a claim the mutant is what
// turns into a measurement.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_field_doorbell.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

using Dut = Vzhao_field_doorbell;

constexpr uint8_t kOpLoad = 0;
constexpr uint8_t kOpCommit = 1;
constexpr uint8_t kOpLookup = 2;

constexpr uint8_t kLdUop = 0;
constexpr uint8_t kLdHeader = 2;

constexpr uint32_t kPlan = 0xF1E1D000u;

void step(Dut& d) { zhao::tick(d); }

// `zhao::reset` drives `in_valid`/`in_data`, which this block does not have.
void reset(Dut& d, int cycles) {
  d.rst_n = 0;
  d.post_valid_i = 0;
  d.ld_ready_i = 0;
  d.pc_lu_ready_i = 0;
  d.pc_lu_resp_valid_i = 0;
  d.pc_lu_hit_i = 0;
  d.pc_lu_slot_i = 0;
  d.pc_cm_ready_i = 0;
  d.pc_cm_resp_valid_i = 0;
  d.pc_cm_inserted_i = 0;
  d.pc_cm_evicted_i = 0;
  d.pc_cm_slot_i = 0;
  d.ret_ready_i = 0;
  d.cfg_plan_base_i = kPlan;
  for (int i = 0; i < cycles; ++i) step(d);
  d.rst_n = 1;
  step(d);
}

void set_post_data(Dut& d, uint64_t lo64, uint32_t hi32) {
  d.post_data_i[0] = static_cast<uint32_t>(lo64 & 0xFFFFFFFFu);
  d.post_data_i[1] = static_cast<uint32_t>(lo64 >> 32);
  d.post_data_i[2] = hi32;
}

// Offer one post and wait for it to be TAKEN. Returns the number of cycles the
// offer was held without being taken -- which is the same thing
// `post_stalls_o` counts, so case 5 can compare two independent views of it.
int post(Dut& d, uint8_t op, uint8_t kind, uint8_t slot, uint8_t addr, uint64_t lo64,
         uint32_t hi32, uint32_t hash, bool ok, uint32_t ticket, int budget = 4000) {
  d.post_valid_i = 1;
  d.post_op_i = op;
  d.post_kind_i = kind;
  d.post_slot_i = slot;
  d.post_addr_i = addr;
  set_post_data(d, lo64, hi32);
  d.post_hash_i = hash;
  d.post_ok_i = ok ? 1 : 0;
  d.post_ticket_i = ticket;
  int held = 0;
  for (int guard = 0; guard < budget; ++guard) {
    d.eval();
    if (d.post_ready_o) {
      step(d);
      d.post_valid_i = 0;
      return held;
    }
    ++held;
    step(d);
  }
  d.post_valid_i = 0;
  return -1;
}

// What a drained return record carries.
struct Ret {
  bool seen = false;
  uint32_t ticket = 0;
  uint8_t op = 0;
  bool ok = false;
  bool refused = false;
  bool inserted = false;
  bool evicted = false;
  uint8_t slot = 0;
  uint32_t plan = 0;
};

// Run the block for up to `budget` cycles, playing the loader and the directory
// and draining at most one return. `lu_hit`/`cm_ins`/`cm_evi`/`resp_slot` are
// what the played directory answers.
//
// THE LOADER'S READY IS PLAYED HIGH and the directory's is too: this file is
// about the mailbox, not about the engine's own back-pressure, and the composed
// article is where those readies come from something real.
Ret service(Dut& d, int budget, bool lu_hit = false, bool cm_ins = false,
            bool cm_evi = false, uint8_t resp_slot = 0, bool drain_returns = true) {
  Ret r;
  d.ld_ready_i = 1;
  d.pc_lu_ready_i = 1;
  d.pc_cm_ready_i = 1;
  d.ret_ready_i = drain_returns ? 1 : 0;
  for (int i = 0; i < budget; ++i) {
    d.eval();
    // The played directory answers one cycle after it takes the request. Both
    // phases are driven from the block's OWN valid, so the response cannot
    // arrive for a request that was never made.
    const bool lu_taken = d.pc_lu_valid_o && d.pc_lu_ready_i;
    const bool cm_taken = d.pc_cm_valid_o && d.pc_cm_ready_i;
    if (drain_returns && d.ret_valid_o && !r.seen) {
      r.seen = true;
      r.ticket = d.ret_ticket_o;
      r.op = d.ret_op_o;
      r.ok = d.ret_ok_o != 0;
      r.refused = d.ret_refused_o != 0;
      r.inserted = d.ret_inserted_o != 0;
      r.evicted = d.ret_evicted_o != 0;
      r.slot = d.ret_slot_o;
      r.plan = d.ret_plan_o;
    }
    step(d);
    d.pc_lu_resp_valid_i = lu_taken ? 1 : 0;
    d.pc_lu_hit_i = lu_hit ? 1 : 0;
    d.pc_lu_slot_i = resp_slot;
    d.pc_cm_resp_valid_i = cm_taken ? 1 : 0;
    d.pc_cm_inserted_i = cm_ins ? 1 : 0;
    d.pc_cm_evicted_i = cm_evi ? 1 : 0;
    d.pc_cm_slot_i = resp_slot;
  }
  d.pc_lu_resp_valid_i = 0;
  d.pc_cm_resp_valid_i = 0;
  return r;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;
  reset(dut, 4);

  // ---- 1. a LOAD WORD reaches the loader VERBATIM -------------------------
  // The fields are distinct primes-ish values rather than zeros, because a
  // forwarding bug that drops a field looks identical to a zero that was never
  // set. The 96-bit payload is checked in all three of Verilator's words.
  const uint64_t kUopLo = 0x0123456789ABCDEFull;
  const uint32_t kUopHi = 0x5A5A1234u;
  bool ld_seen = false;
  uint8_t ld_kind = 0xFF, ld_slot = 0xFF, ld_addr = 0xFF;
  uint32_t ld_d0 = 0, ld_d1 = 0, ld_d2 = 0;

  check(post(dut, kOpLoad, kLdUop, 5, 0x2A, kUopLo, kUopHi, 0, false, 0x1001) >= 0,
        "the mailbox took a load-word post", 1, 1);
  {
    dut.ld_ready_i = 1;
    for (int i = 0; i < 40 && !ld_seen; ++i) {
      dut.eval();
      if (dut.ld_valid_o) {
        ld_seen = true;
        ld_kind = dut.ld_kind_o;
        ld_slot = dut.ld_slot_o;
        ld_addr = dut.ld_addr_o;
        ld_d0 = dut.ld_data_o[0];
        ld_d1 = dut.ld_data_o[1];
        ld_d2 = dut.ld_data_o[2];
      }
      step(dut);
    }
    dut.ld_ready_i = 0;
  }
  check(ld_seen, "the load word reached the loader port", 1, ld_seen ? 1 : 0);
  check(ld_kind == kLdUop, "ld_kind_o carried", kLdUop, ld_kind);
  check(ld_slot == 5, "ld_slot_o carried", 5, ld_slot);
  check(ld_addr == 0x2A, "ld_addr_o carried", 0x2A, ld_addr);
  check(ld_d0 == static_cast<uint32_t>(kUopLo & 0xFFFFFFFFu), "ld_data_o[31:0] carried",
        static_cast<int>(kUopLo & 0xFFFFFFFFu), static_cast<int>(ld_d0));
  check(ld_d1 == static_cast<uint32_t>(kUopLo >> 32), "ld_data_o[63:32] carried",
        static_cast<int>(kUopLo >> 32), static_cast<int>(ld_d1));
  check(ld_d2 == kUopHi, "ld_data_o[95:64] carried", static_cast<int>(kUopHi),
        static_cast<int>(ld_d2));
  check(dut.load_words_o == 1, "load_words_o fired", 1, dut.load_words_o);
  check(dut.posts_o == 1, "posts_o counted the post", 1, dut.posts_o);

  // ---- 2. a LOOKUP reaches the directory and its ANSWER returns -----------
  // A MISS. `ok` and `inserted` apart is the whole point: "the directory
  // answered" and "the hash is resident" are different facts, and merging them
  // would make a miss indistinguishable from a doorbell that never asked.
  check(post(dut, kOpLookup, 0, 0, 0, 0, 0, 0xFEEDBEEFu, false, 0x2002) >= 0,
        "the mailbox took a lookup post", 1, 1);
  Ret lu = service(dut, 40, /*lu_hit=*/false, false, false, /*resp_slot=*/3);
  check(dut.lookups_o == 1, "lookups_o fired", 1, dut.lookups_o);
  check(lu.seen, "the lookup was ANSWERED", 1, lu.seen ? 1 : 0);
  check(lu.ticket == 0x2002u, "the return carried the lookup's own ticket", 0x2002,
        static_cast<int>(lu.ticket));
  check(lu.op == kOpLookup, "the return says which post it answers", kOpLookup, lu.op);
  check(lu.ok, "the return says the directory answered", 1, lu.ok ? 1 : 0);
  check(!lu.inserted, "a hash nothing loaded is NOT reported resident", 0,
        lu.inserted ? 1 : 0);
  check(lu.plan == kPlan, "the return carried the plan it belongs to",
        static_cast<int>(kPlan), static_cast<int>(lu.plan));

  // ---- 4 (before 3, because order is the law under test) ------------------
  //      a COMMIT with NO HEADER is REFUSED, ANSWERED and COUNTED.
  //
  // This is the block's reason for existing rather than being a FIFO, and it
  // is ruling R20's shape: the directory is never offered the hash, the HPS
  // gets a record back, and a counter moves. Slot 5 got a UOP in case 1 and no
  // header, so its runnable bit is clear -- which is exactly the state
  // `zhao_field_host` would refuse a RUN in, one step later and too late to
  // stop the directory promising the hash.
  const uint32_t commits_before = dut.commits_o;
  check(post(dut, kOpCommit, 0, 5, 0, 0, 0, 0xC0FFEE01u, true, 0x3003) >= 0,
        "the mailbox took a headerless commit", 1, 1);
  Ret cm_bad = service(dut, 40, false, /*cm_ins=*/true, false, /*resp_slot=*/5);
  check(dut.commits_refused_o == 1, "commits_refused_o FIRED", 1, dut.commits_refused_o);
  check(dut.commits_o == commits_before,
        "the directory was never offered the hash", static_cast<int>(commits_before),
        static_cast<int>(dut.commits_o));
  check(cm_bad.seen, "the refusal was ANSWERED, never hung", 1, cm_bad.seen ? 1 : 0);
  check(cm_bad.refused, "the return says REFUSED", 1, cm_bad.refused ? 1 : 0);
  check(!cm_bad.ok, "a refusal does not claim the directory answered", 0,
        cm_bad.ok ? 1 : 0);
  check(cm_bad.ticket == 0x3003u, "the refusal carried its own ticket", 0x3003,
        static_cast<int>(cm_bad.ticket));

  // ---- 3. a HEADER then a COMMIT reaches the directory --------------------
  check(post(dut, kOpLoad, kLdHeader, 5, 0, 0x0000000000000102ull, 0, 0, false, 0x4004) >= 0,
        "the mailbox took a header post", 1, 1);
  service(dut, 20);
  check(dut.load_words_o == 2, "the header was handed to the loader", 2, dut.load_words_o);

  check(post(dut, kOpCommit, 0, 5, 0, 0, 0, 0xC0FFEE02u, true, 0x5005) >= 0,
        "the mailbox took a commit", 1, 1);
  Ret cm_ok = service(dut, 40, false, /*cm_ins=*/true, /*cm_evi=*/true, /*resp_slot=*/5);
  check(dut.commits_o == commits_before + 1, "commits_o fired",
        static_cast<int>(commits_before + 1), static_cast<int>(dut.commits_o));
  check(dut.commits_refused_o == 1, "a commit WITH a header is not refused", 1,
        dut.commits_refused_o);
  check(cm_ok.seen, "the commit was answered", 1, cm_ok.seen ? 1 : 0);
  check(cm_ok.ok, "the return says the directory answered", 1, cm_ok.ok ? 1 : 0);
  check(cm_ok.inserted, "the return carried the insert", 1, cm_ok.inserted ? 1 : 0);
  check(cm_ok.evicted, "the return carried the eviction", 1, cm_ok.evicted ? 1 : 0);
  check(cm_ok.slot == 5, "the return carried the directory's slot", 5, cm_ok.slot);
  check(cm_ok.ticket == 0x5005u, "the return carried the commit's ticket", 0x5005,
        static_cast<int>(cm_ok.ticket));

  // ---- 6. an INSERT SPENDS THE HEADER -------------------------------------
  // The commit above inserted, so slot 5's runnable bit was cleared -- mirroring
  // what `zhao_field_host` does on the same event. A SECOND commit for the same
  // slot, with no new header, must therefore be refused. Without this the
  // directory could promise one header's microcode to two different hashes.
  check(post(dut, kOpCommit, 0, 5, 0, 0, 0, 0xC0FFEE03u, true, 0x6006) >= 0,
        "the mailbox took the second commit", 1, 1);
  Ret cm_again = service(dut, 40, false, true, false, 5);
  check(dut.commits_refused_o == 2, "the second commit was refused too", 2,
        dut.commits_refused_o);
  check(cm_again.seen && cm_again.refused, "and it was answered as refused", 1,
        (cm_again.seen && cm_again.refused) ? 1 : 0);

  // ---- 5. a post into a FULL MAILBOX is HELD, NEVER DROPPED ---------------
  // POSTS is 4. Five posts are offered with the loader's ready LOW, so the
  // drain cannot move: the fifth must be held, `post_stalls_o` must count the
  // cycles it was held, and when the drain resumes ALL FIVE must reach the
  // loader. A mailbox that dropped the fifth would show the same
  // `post_ready_o` waveform and a smaller `load_words_o`, which is the silent
  // direction ruling R55 is about.
  const uint32_t words_before = dut.load_words_o;
  const uint32_t stalls_before = dut.post_stalls_o;
  dut.ld_ready_i = 0;
  dut.pc_lu_ready_i = 0;
  dut.pc_cm_ready_i = 0;
  int accepted = 0;
  int held_cycles = 0;
  for (int i = 0; i < 5; ++i) {
    dut.post_valid_i = 1;
    dut.post_op_i = kOpLoad;
    dut.post_kind_i = kLdUop;
    dut.post_slot_i = 1;
    dut.post_addr_i = static_cast<uint8_t>(i);
    set_post_data(dut, 0xA5A5000000000000ull + i, 0);
    dut.post_hash_i = 0;
    dut.post_ok_i = 0;
    dut.post_ticket_i = 0x7000u + i;
    bool taken = false;
    for (int guard = 0; guard < 24 && !taken; ++guard) {
      dut.eval();
      if (dut.post_ready_o) {
        taken = true;
        ++accepted;
      } else {
        ++held_cycles;
      }
      step(dut);
    }
    if (!taken) break;
  }
  dut.post_valid_i = 0;
  check(accepted == 4, "exactly POSTS posts fit while the drain is stopped", 4, accepted);
  check(held_cycles > 0, "the fifth post was HELD", 1, held_cycles > 0 ? 1 : 0);
  check(dut.post_stalls_o > stalls_before, "post_stalls_o FIRED",
        1, dut.post_stalls_o > stalls_before ? 1 : 0);

  // Now let the drain run and offer the fifth again. Nothing was lost.
  service(dut, 80);
  check(post(dut, kOpLoad, kLdUop, 1, 4, 0xA5A5000000000004ull, 0, 0, false, 0x7004) >= 0,
        "the fifth post is taken once there is room", 1, 1);
  service(dut, 80);
  check(dut.load_words_o == words_before + 5,
        "all five load words reached the loader -- none was dropped",
        static_cast<int>(words_before + 5), static_cast<int>(dut.load_words_o));

  // ---- 7. THE RETURN QUEUE NEVER OVERFLOWED, and that is a CLAIM ----------
  // Unreachable while the credit is right, so no stimulus in this file can move
  // it. `tests/mutants/zhao_field_doorbell_mutant.sv` is what turns this zero
  // into a measurement; quoting it alone would be the broken-instrument law.
  check(dut.ret_overflow_o == 0, "ret_overflow_o stayed zero (see the mutant)", 0,
        dut.ret_overflow_o);

  // ---- 8. THE MUTANT'S NEGATIVE CONTROL, RUN HERE -------------------------
  // The SAME stimulus `field_doorbell_mutant_control` uses -- twelve lookups
  // offered while the HPS NEVER DRAINS -- against UNMUTATED production. With
  // the credit present the mailbox simply backs up and nothing overflows.
  //
  // Both halves are required before the zero above may be cited: the mutant
  // shows the counter CAN fire, and this shows the same stimulus does NOT fire
  // it when the guard is right. A positive control without its negative half
  // proves only that some circuit somewhere can increment a register.
  const uint32_t lookups_before = dut.lookups_o;
  dut.ret_ready_i = 0;   // the HPS stops draining
  dut.ld_ready_i = 1;
  dut.pc_lu_ready_i = 1;
  dut.pc_cm_ready_i = 1;
  int nd_posted = 0;
  for (int cycle = 0; cycle < 2000; ++cycle) {
    dut.post_valid_i = (nd_posted < 12) ? 1 : 0;
    dut.post_op_i = kOpLookup;
    dut.post_kind_i = 0;
    dut.post_slot_i = 0;
    dut.post_addr_i = 0;
    set_post_data(dut, 0, 0);
    dut.post_hash_i = 0xFEEDBEEFu + nd_posted;
    dut.post_ok_i = 0;
    dut.post_ticket_i = 0x9000u + nd_posted;
    dut.eval();
    const bool took = dut.post_valid_i && dut.post_ready_o;
    const bool lu_taken = dut.pc_lu_valid_o && dut.pc_lu_ready_i;
    step(dut);
    if (took) ++nd_posted;
    dut.pc_lu_resp_valid_i = lu_taken ? 1 : 0;
    dut.pc_lu_hit_i = 0;
    dut.pc_lu_slot_i = 0;
  }
  dut.post_valid_i = 0;
  dut.pc_lu_resp_valid_i = 0;
  std::printf("field_doorbell_directed: negative control posted=%d lookups=%u "
              "ret_overflow_o=%u\n",
              nd_posted, static_cast<unsigned>(dut.lookups_o - lookups_before),
              static_cast<unsigned>(dut.ret_overflow_o));
  check(dut.ret_overflow_o == 0,
        "NEGATIVE CONTROL: the mutant's own stimulus does NOT overflow unmutated "
        "production -- the credit is what stops it",
        0, dut.ret_overflow_o);
  check(dut.lookups_o - lookups_before <= 4u,
        "and the credit STOPPED the drain at RETQ rather than letting it run on",
        1, (dut.lookups_o - lookups_before <= 4u) ? 1 : 0);
  check(nd_posted < 12,
        "the mailbox backed up instead of swallowing every post", 1,
        nd_posted < 12 ? 1 : 0);

  return zhao::report_and_exit("field_doorbell_directed");
}
