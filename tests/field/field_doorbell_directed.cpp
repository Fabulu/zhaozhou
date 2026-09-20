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
// FH14. Op 3 used to fall into the catch-all LOAD arm; it is now the FH2
// transaction and is decoded by name.
constexpr uint8_t kOpFh2 = 3;

constexpr uint8_t kLdUop = 0;
constexpr uint8_t kLdHeader = 2;

// FH2 kinds, owner directive section 10.2.
constexpr uint8_t kFh2Install = 0;
constexpr uint8_t kFh2Bind = 1;
constexpr uint8_t kFh2Control = 2;
constexpr uint8_t kFh2Reserved = 3;

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
  d.fh2_ready_i = 0;
  d.fh2_resp_valid_i = 0;
  d.fh2_resp_ok_i = 0;
  d.fh2_resp_verdict_i = 0;
  d.fh2_resp_handle_i = 0;
  d.fh2_resp_slot_i = 0;
  d.fh2_resp_evicted_i = 0;
  for (int i = 0; i < cycles; ++i) step(d);
  d.rst_n = 1;
  step(d);
}

// What the FH2 seam was offered, captured the cycle the loader took it. A
// struct rather than loose globals so a case can assert on the WHOLE offer --
// a forwarding bug that drops one field looks exactly like a field that was
// never set, which is case 1's lesson applied to the new port.
struct Fh2Offer {
  bool seen = false;
  uint8_t kind = 0xFF;
  uint8_t verb = 0xFF;
  uint32_t d0 = 0, d1 = 0, d2 = 0;
  uint32_t hash = 0;
  uint32_t ticket = 0;
  uint32_t plan = 0;
};

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
  uint8_t verdict = 0;
  uint32_t handle = 0;
};

// Run the block for up to `budget` cycles, playing the loader and the directory
// and draining at most one return. `lu_hit`/`cm_ins`/`cm_evi`/`resp_slot` are
// what the played directory answers.
//
// THE LOADER'S READY IS PLAYED HIGH and the directory's is too: this file is
// about the mailbox, not about the engine's own back-pressure, and the composed
// article is where those readies come from something real.
Ret service(Dut& d, int budget, bool lu_hit = false, bool cm_ins = false,
            bool cm_evi = false, uint8_t resp_slot = 0, bool drain_returns = true,
            Fh2Offer* fh2 = nullptr, bool fh2_ok = true, uint8_t fh2_verdict = 0,
            uint32_t fh2_handle = 0, bool fh2_evicted = false) {
  Ret r;
  d.ld_ready_i = 1;
  d.pc_lu_ready_i = 1;
  d.pc_cm_ready_i = 1;
  // The played LOADER is always ready, exactly as the played directory is.
  d.fh2_ready_i = 1;
  d.ret_ready_i = drain_returns ? 1 : 0;
  for (int i = 0; i < budget; ++i) {
    d.eval();
    // The played directory answers one cycle after it takes the request. Both
    // phases are driven from the block's OWN valid, so the response cannot
    // arrive for a request that was never made. The FH2 responder is built the
    // same way and for the same reason.
    const bool lu_taken = d.pc_lu_valid_o && d.pc_lu_ready_i;
    const bool cm_taken = d.pc_cm_valid_o && d.pc_cm_ready_i;
    const bool fh2_taken = d.fh2_valid_o && d.fh2_ready_i;
    if (fh2_taken && fh2 != nullptr && !fh2->seen) {
      fh2->seen = true;
      fh2->kind = d.fh2_kind_o;
      fh2->verb = d.fh2_verb_o;
      fh2->d0 = d.fh2_data_o[0];
      fh2->d1 = d.fh2_data_o[1];
      fh2->d2 = d.fh2_data_o[2];
      fh2->hash = d.fh2_hash_o;
      fh2->ticket = d.fh2_ticket_o;
      fh2->plan = d.fh2_plan_o;
    }
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
      r.verdict = d.ret_verdict_o;
      r.handle = d.ret_handle_o;
    }
    step(d);
    d.pc_lu_resp_valid_i = lu_taken ? 1 : 0;
    d.pc_lu_hit_i = lu_hit ? 1 : 0;
    d.pc_lu_slot_i = resp_slot;
    d.pc_cm_resp_valid_i = cm_taken ? 1 : 0;
    d.pc_cm_inserted_i = cm_ins ? 1 : 0;
    d.pc_cm_evicted_i = cm_evi ? 1 : 0;
    d.pc_cm_slot_i = resp_slot;
    d.fh2_resp_valid_i = fh2_taken ? 1 : 0;
    d.fh2_resp_ok_i = fh2_ok ? 1 : 0;
    d.fh2_resp_verdict_i = fh2_verdict;
    d.fh2_resp_handle_i = fh2_handle;
    d.fh2_resp_slot_i = resp_slot;
    d.fh2_resp_evicted_i = fh2_evicted ? 1 : 0;
  }
  d.pc_lu_resp_valid_i = 0;
  d.pc_cm_resp_valid_i = 0;
  d.fh2_resp_valid_i = 0;
  return r;
}

// Run the block until the mailbox AND the return queue are both empty.
//
// Case 8 deliberately leaves posts queued and returns undrained -- that is its
// subject. Without this, the next case's `service()` would capture whichever
// stale return happened to be at the head, and its assertions would be about a
// post from a previous case. That is not a hypothetical: FT060 first reported
// `ret_op_o == 2` for an op-3 post, which was case 8's last lookup answering
// on FT060's behalf.
void drain_all(Dut& d, int budget = 4000) {
  d.ld_ready_i = 1;
  d.pc_lu_ready_i = 1;
  d.pc_cm_ready_i = 1;
  d.fh2_ready_i = 1;
  d.ret_ready_i = 1;
  d.post_valid_i = 0;
  int quiet = 0;
  for (int i = 0; i < budget && quiet < 12; ++i) {
    d.eval();
    const bool lu_taken = d.pc_lu_valid_o && d.pc_lu_ready_i;
    const bool cm_taken = d.pc_cm_valid_o && d.pc_cm_ready_i;
    const bool fh2_taken = d.fh2_valid_o && d.fh2_ready_i;
    const bool busy = d.ret_valid_o || d.ld_valid_o || d.pc_lu_valid_o ||
                      d.pc_cm_valid_o || d.fh2_valid_o;
    quiet = busy ? 0 : (quiet + 1);
    step(d);
    d.pc_lu_resp_valid_i = lu_taken ? 1 : 0;
    d.pc_lu_hit_i = 0;
    d.pc_lu_slot_i = 0;
    d.pc_cm_resp_valid_i = cm_taken ? 1 : 0;
    d.pc_cm_inserted_i = 0;
    d.pc_cm_evicted_i = 0;
    d.pc_cm_slot_i = 0;
    d.fh2_resp_valid_i = fh2_taken ? 1 : 0;
    d.fh2_resp_ok_i = 1;
    d.fh2_resp_verdict_i = 0;
    d.fh2_resp_handle_i = 0;
    d.fh2_resp_evicted_i = 0;
  }
  d.pc_lu_resp_valid_i = 0;
  d.pc_cm_resp_valid_i = 0;
  d.fh2_resp_valid_i = 0;
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

  drain_all(dut);

  // ---- FT060. OP 3 REACHES FH2, NEVER THE CATCH-ALL LOAD ARM -------------
  //
  // Owner decision FH14: "The old doorbell's catch-all LOAD arm currently
  // catches post_op=3. Replace it with exhaustive decoding before introducing
  // the extension. Unknown operations must return a counted refusal, never
  // become a load."
  //
  // THE ASSERTION THAT MATTERS IS THE NEGATIVE ONE. `load_words_o` must not
  // move: under the old arrangement an op-3 post performed a real loader write
  // with whatever `post_kind_i` carried, and moved the header shadow with it.
  // `tests/mutants/zhao_field_doorbell_op3_alias_mutant.sv` restores that
  // arrangement and `field_doorbell_op3_alias_control` requires this case to
  // FAIL against it.
  {
    const uint32_t words_b = dut.load_words_o;
    const uint32_t cmts_b = dut.commits_o;
    const uint32_t lkps_b = dut.lookups_o;
    const uint32_t fh2_b = dut.fh2_posts_o;

    // Distinct non-zero fields again, for the same reason as case 1.
    const uint64_t kInsLo = 0x00000000DEADBE00ull;  // HPS source address
    const uint32_t kInsHi = 0x00001000u;            // byte length
    const uint32_t kInsCrc = 0x9E3779B9u;  // the capsule's expected CRC32C
    check(post(dut, kOpFh2, kFh2Install, 0, 0x5, kInsLo, kInsHi, kInsCrc, false,
               0x8001) >= 0,
          "the mailbox took an op-3 INSTALL post", 1, 1);

    Fh2Offer off;
    Ret rf = service(dut, 60, false, false, false, /*resp_slot=*/2, true, &off,
                     /*fh2_ok=*/true, /*fh2_verdict=*/0, /*fh2_handle=*/0x0107u);

    check(off.seen, "op 3 REACHED THE FH2 SEAM", 1, off.seen ? 1 : 0);
    check(dut.load_words_o == words_b,
          "and NO LOADER WRITE happened -- the catch-all arm is gone",
          static_cast<int>(words_b), static_cast<int>(dut.load_words_o));
    check(dut.commits_o == cmts_b, "op 3 did not reach the commit phase",
          static_cast<int>(cmts_b), static_cast<int>(dut.commits_o));
    check(dut.lookups_o == lkps_b, "op 3 did not reach the lookup phase",
          static_cast<int>(lkps_b), static_cast<int>(dut.lookups_o));
    check(dut.fh2_posts_o == fh2_b + 1, "fh2_posts_o FIRED",
          static_cast<int>(fh2_b + 1), static_cast<int>(dut.fh2_posts_o));

    check(off.kind == kFh2Install, "the FH2 kind carried", kFh2Install, off.kind);
    check(off.verb == 0x5, "the 8-bit FH2 verb carried from post_addr_i", 0x5, off.verb);
    check(off.d0 == static_cast<uint32_t>(kInsLo & 0xFFFFFFFFu),
          "FH2 data[31:0] carried", static_cast<int>(kInsLo & 0xFFFFFFFFu),
          static_cast<int>(off.d0));
    check(off.d2 == kInsHi, "FH2 data[95:64] carried", static_cast<int>(kInsHi),
          static_cast<int>(off.d2));
    check(off.ticket == 0x8001u, "the FH2 offer carried its own ticket", 0x8001,
          static_cast<int>(off.ticket));
    check(off.hash == kInsCrc, "the expected-CRC operand carried",
          static_cast<int>(kInsCrc), static_cast<int>(off.hash));
    check(off.plan == kPlan, "and the offer carried the plan captured AT ACCEPTANCE",
          static_cast<int>(kPlan), static_cast<int>(off.plan));

    check(rf.seen, "the FH2 command was ANSWERED", 1, rf.seen ? 1 : 0);
    check(rf.op == kOpFh2, "the return says it answers an op-3 post", kOpFh2, rf.op);
    check(rf.handle == 0x0107u, "the return carried the loader's binding HANDLE",
          0x0107, static_cast<int>(rf.handle));
  }

  drain_all(dut);

  // ---- FT060b. AN UNKNOWN FH2 KIND WRITES NOTHING -------------------------
  // "Unknown kind/verb receives a refusal and no uop/table/scalar write."
  // Kind 3 under op 3 is RESERVED. The doorbell forwards it and the loader
  // answers BAD_OPERATION; what this file owns is the other half -- that the
  // reserved kind never becomes a load on the way there.
  {
    const uint32_t words_b = dut.load_words_o;
    check(post(dut, kOpFh2, kFh2Reserved, 0, 0, 0, 0, 0, false, 0x8002) >= 0,
          "the mailbox took an op-3 RESERVED-kind post", 1, 1);
    Fh2Offer off;
    Ret rf = service(dut, 60, false, false, false, 0, true, &off,
                     /*fh2_ok=*/false, /*fh2_verdict=*/8 /* V_BAD_OPERATION */);
    check(off.seen && off.kind == kFh2Reserved,
          "the reserved kind reached the loader to be refused THERE", 1,
          (off.seen && off.kind == kFh2Reserved) ? 1 : 0);
    check(dut.load_words_o == words_b,
          "an unknown kind performed NO uop/table/header/uniform write",
          static_cast<int>(words_b), static_cast<int>(dut.load_words_o));
    check(rf.seen && rf.refused, "and it was ANSWERED as refused", 1,
          (rf.seen && rf.refused) ? 1 : 0);
    check(rf.verdict == 8, "the refusal carried V_BAD_OPERATION", 8, rf.verdict);
  }

  drain_all(dut);

  // ---- FT061. TWO QUEUED POSTS KEEP THEIR OWN PLAN METADATA ---------------
  //
  // "Two queued mailbox posts with different plan/epoch metadata retain their
  // own metadata while the live cfg pins change before either is drained."
  //
  // THIS IS THE CASE THE OLD BLOCK FAILED. `q_ticket` was captured at intake
  // while the plan was read LIVE off `cfg_plan_base_i` at drain, so both
  // returns carried whatever plan happened to be on the pin when the drain got
  // to them -- post A's ticket paired with post B's plan.
  //
  // The stimulus is built so the two readings give DIFFERENT answers, which is
  // the only way the case can discriminate: both posts are queued with the
  // drain stopped, each under its own plan value, and the pin is then moved
  // AGAIN to a third value before either is drained. Correct behaviour returns
  // planA and planB; the defect returns planC twice.
  {
    const uint32_t kPlanA = 0xAAAA0001u;
    const uint32_t kPlanB = 0xBBBB0002u;
    const uint32_t kPlanC = 0xCCCC0003u;

    // Stop the drain so both posts are definitely resident together.
    dut.ld_ready_i = 0;
    dut.pc_lu_ready_i = 0;
    dut.pc_cm_ready_i = 0;
    dut.fh2_ready_i = 0;
    dut.ret_ready_i = 0;

    dut.cfg_plan_base_i = kPlanA;
    check(post(dut, kOpLookup, 0, 0, 0, 0, 0, 0x1111AAAAu, false, 0xA001) >= 0,
          "post A queued under plan A", 1, 1);
    dut.cfg_plan_base_i = kPlanB;
    check(post(dut, kOpLookup, 0, 0, 0, 0, 0, 0x2222BBBBu, false, 0xB002) >= 0,
          "post B queued under plan B", 1, 1);

    // The live pin moves AGAIN, to a value neither post ever saw. Under the
    // old code this is the value both returns would carry.
    dut.cfg_plan_base_i = kPlanC;
    for (int i = 0; i < 6; ++i) step(dut);

    uint32_t plan_a = 0xFFFFFFFFu, plan_b = 0xFFFFFFFFu;
    uint32_t tick_a = 0, tick_b = 0;
    int drained = 0;
    dut.ld_ready_i = 1;
    dut.pc_lu_ready_i = 1;
    dut.pc_cm_ready_i = 1;
    dut.fh2_ready_i = 1;
    dut.ret_ready_i = 1;
    for (int i = 0; i < 200 && drained < 2; ++i) {
      dut.eval();
      const bool lu_taken = dut.pc_lu_valid_o && dut.pc_lu_ready_i;
      if (dut.ret_valid_o && dut.ret_ready_i) {
        if (drained == 0) {
          tick_a = dut.ret_ticket_o;
          plan_a = dut.ret_plan_o;
        } else {
          tick_b = dut.ret_ticket_o;
          plan_b = dut.ret_plan_o;
        }
        ++drained;
      }
      step(dut);
      dut.pc_lu_resp_valid_i = lu_taken ? 1 : 0;
      dut.pc_lu_hit_i = 0;
      dut.pc_lu_slot_i = 0;
    }
    dut.pc_lu_resp_valid_i = 0;

    std::printf("field_doorbell_directed: FT061 drained=%d A{t=%08X p=%08X} "
                "B{t=%08X p=%08X} live pin=%08X\n",
                drained, tick_a, plan_a, tick_b, plan_b, kPlanC);

    check(drained == 2, "both queued posts were answered", 2, drained);
    check(tick_a == 0xA001u, "the first return is post A's", 0xA001,
          static_cast<int>(tick_a));
    check(tick_b == 0xB002u, "the second return is post B's", 0xB002,
          static_cast<int>(tick_b));
    // The three discriminating assertions. Each fails under the live-pin
    // defect, and the third is the one that cannot be satisfied by accident.
    check(plan_a == kPlanA,
          "post A's return carries the plan that was live WHEN A WAS POSTED",
          static_cast<int>(kPlanA), static_cast<int>(plan_a));
    check(plan_b == kPlanB,
          "post B's return carries the plan that was live WHEN B WAS POSTED",
          static_cast<int>(kPlanB), static_cast<int>(plan_b));
    check(plan_a != plan_b,
          "and the two returns DISAGREE -- the defect makes them identical", 1,
          (plan_a != plan_b) ? 1 : 0);
    check(plan_a != kPlanC && plan_b != kPlanC,
          "neither return carries the plan that was merely live at DRAIN time", 1,
          (plan_a != kPlanC && plan_b != kPlanC) ? 1 : 0);

    dut.cfg_plan_base_i = kPlan;
  }

  drain_all(dut);

  // ---- A LEGACY LOAD ADDRESS THAT DOES NOT FIT IS REFUSED, NOT TRUNCATED --
  // POSTADDRW is 8 and LDADDRW is 7, so address 0x80 is expressible in the
  // mailbox and not on the loader. Truncating it would write microcode address
  // 0x00 -- a wrong word at a plausible address, which is the worst available
  // outcome. Reachable with legal stimulus, so this counter needs no mutant.
  {
    const uint32_t words_b = dut.load_words_o;
    const uint32_t refused_b = dut.addr_refused_o;
    check(post(dut, kOpLoad, kLdUop, 1, 0x80, 0x1234ull, 0, 0, false, 0xC001) >= 0,
          "the mailbox took a load post with an 8-bit address", 1, 1);
    Ret ra = service(dut, 60);
    check(dut.addr_refused_o == refused_b + 1, "addr_refused_o FIRED",
          static_cast<int>(refused_b + 1), static_cast<int>(dut.addr_refused_o));
    check(dut.load_words_o == words_b,
          "and the loader was never offered the truncated address",
          static_cast<int>(words_b), static_cast<int>(dut.load_words_o));
    check(ra.seen && ra.refused, "the over-wide address was ANSWERED as refused", 1,
          (ra.seen && ra.refused) ? 1 : 0);

    // The negative control: 0x7F is the largest address that DOES fit, and it
    // must pass. Without this, a block that refused every load would score the
    // same on the assertion above.
    const uint32_t words_c = dut.load_words_o;
    const uint32_t refused_c = dut.addr_refused_o;
    check(post(dut, kOpLoad, kLdUop, 1, 0x7F, 0x5678ull, 0, 0, false, 0xC002) >= 0,
          "the mailbox took a load post at the widest FITTING address", 1, 1);
    service(dut, 60);
    check(dut.load_words_o == words_c + 1,
          "NEGATIVE CONTROL: address 0x7F is accepted and reaches the loader",
          static_cast<int>(words_c + 1), static_cast<int>(dut.load_words_o));
    check(dut.addr_refused_o == refused_c,
          "and it was not counted as a refusal", static_cast<int>(refused_c),
          static_cast<int>(dut.addr_refused_o));
  }

  return zhao::report_and_exit("field_doorbell_directed");
}
