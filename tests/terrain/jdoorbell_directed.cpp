// jdoorbell_directed.cpp -- TERRAIN.WRITEBACK's journal doorbell (owner ruling R14).
//
// Contract: design/contracts/TERRAIN.WRITEBACK.DOORBELL.md.
//
// WHAT THIS GUARDS. The doorbell attaches SW.STREAM's {journal address, ticket}
// to TERRAIN.SEQ's writeback jobs and hands every ticket back. Its failures are
// all SILENT in a result-only test:
//   * a job that goes out WITHOUT a grant carries a ticket the hardware made up
//     -- the exact glue R14 replaces. Case 2 holds a job with no grant posted and
//     requires that nothing leaves and the wait is counted;
//   * a grant consumed out of order journals sheet A into entry B, and the
//     journal then restores the wrong patch's scars. Case 3 checks order;
//   * a ticket never returned FINAL leaks a journal entry for ever; one
//     returned twice lets software reuse an entry that is still being written.
//     Cases 4 and 5 count returns per ticket exactly;
//   * the return queue overflowing drops a LANDED, so software never ACKs and
//     the slot stays EVICT_PENDING for ever. The credit (case 6) is what makes
//     that impossible, so `ret_overflow_o` cannot be reached with legal stimulus
//     and is fired by the committed mutant instead
//     (tests/terrain/jdoorbell_mutant_control.cpp).
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_terrain_jdoorbell.h"

#include "zhao_sim.hpp"

namespace {

int failures = 0;
int checks = 0;

void check_eq(uint64_t got, uint64_t want, const char* what) {
  ++checks;
  if (got != want) {
    std::printf("FAIL: %s -- expected 0x%llx, got 0x%llx\n", what,
                static_cast<unsigned long long>(want), static_cast<unsigned long long>(got));
    ++failures;
  }
}

constexpr uint32_t kBase = 0x30000000u;
constexpr uint32_t kFBytes = 8192u;
constexpr int kTickets = 4;  // the module default under test
constexpr int kGrants = 4;

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_jdoorbell t;

  auto quiet = [&]() {
    t.post_valid_i = 0;
    t.sj_valid_i = 0;
    t.wj_ready_i = 0;
    t.landed_valid_i = 0;
    t.done_valid_i = 0;
    t.ret_ready_i = 0;
  };
  auto reset = [&]() {
    quiet();
    t.cfg_journal_base_i = kBase;
    t.rst_n = 0;
    for (int i = 0; i < 3; ++i) zhao::tick(t);
    t.rst_n = 1;
    t.eval();
  };
  auto post = [&](uint32_t slot, uint32_t ticket) {
    t.post_valid_i = 1;
    t.post_slot_i = slot;
    t.post_ticket_i = ticket;
    t.eval();
    const bool took = t.post_ready_o;
    zhao::tick(t);
    t.post_valid_i = 0;
    t.eval();
    return took;
  };
  auto offer_job = [&](uint32_t slot) {
    t.sj_valid_i = 1;
    t.sj_slot_i = slot;
    t.sj_gen_i = (slot * 3u) & 0xFFu;
    t.sj_epoch_i = 0xE0000000u | slot;
    t.sj_island_i = 0x15000000u | slot;
    t.sj_ix_i = static_cast<uint16_t>(slot + 7);
    t.sj_iz_i = static_cast<uint16_t>(0xFFF0u - slot);  // negative: -16 - slot
    t.sj_src_id_i = 0x5C000000u | slot;
    t.eval();
  };
  // Accept one job; returns the ticket the writeback would have captured.
  auto take_job = [&](uint32_t slot, uint64_t* addr) {
    offer_job(slot);
    t.wj_ready_i = 1;
    t.eval();
    const uint32_t seq = t.wj_seq_o;
    if (addr) *addr = t.wj_journal_addr_o;
    const bool fired = t.sj_ready_o && t.wj_valid_o;
    zhao::tick(t);
    t.sj_valid_i = 0;
    t.wj_ready_i = 0;
    t.eval();
    return fired ? seq : 0xDEADu;
  };
  auto landed = [&](uint32_t ticket) {
    t.landed_valid_i = 1;
    t.landed_seq_i = ticket;
    t.eval();
    zhao::tick(t);
    t.landed_valid_i = 0;
    t.eval();
  };
  auto done = [&](uint32_t slot, uint32_t ticket, bool ok, uint32_t verdict) {
    t.done_valid_i = 1;
    t.done_slot_i = slot;
    t.done_seq_i = ticket;
    t.done_ok_i = ok;
    t.done_verdict_i = verdict;
    t.eval();
    check_eq(t.seq_done_valid_o, 1, "the completion reaches TERRAIN.SEQ on the same cycle");
    check_eq(t.seq_done_slot_o, slot, "...naming the writeback's slot");
    check_eq(t.done_ready_o, 1, "a completion is always taken");
    zhao::tick(t);
    t.done_valid_i = 0;
    t.eval();
  };
  struct Ret {
    uint32_t ticket, final_, ok, verdict;
  };
  auto pop = [&]() {
    Ret r{0xFFFFFFFFu, 9, 9, 99};
    if (!t.ret_valid_o) return r;
    r = Ret{t.ret_ticket_o, t.ret_final_o, t.ret_ok_o, t.ret_verdict_o};
    t.ret_ready_i = 1;
    t.eval();
    zhao::tick(t);
    t.ret_ready_i = 0;
    t.eval();
    return r;
  };

  // ---- case 1: the address law and field transport ------------------------
  reset();
  check_eq(t.wj_valid_o, 0, "1 nothing leaves before a job is offered");
  check_eq(post(5, 0x77u), 1, "1 a grant is taken into an empty mailbox");
  offer_job(0x2A);
  check_eq(t.wj_valid_o, 1, "1 a job with a grant posted is offered on");
  check_eq(t.wj_journal_addr_o, uint64_t{kBase} + 5ull * kFBytes,
           "1 address = journal_base + slot * 8192 (contract law 1)");
  check_eq(t.wj_seq_o, 0x77u, "1 the ticket is the grant's, not invented");
  check_eq(t.wj_slot_o, 0x2A, "1 slot passes through");
  check_eq(t.wj_gen_o, (0x2Au * 3u) & 0xFFu, "1 generation passes through");
  check_eq(t.wj_epoch_o, 0xE000002Au, "1 epoch passes through");
  check_eq(t.wj_island_o, 0x1500002Au, "1 island passes through");
  check_eq(static_cast<uint16_t>(t.wj_ix_o), 0x2Au + 7u, "1 ix passes through");
  check_eq(static_cast<uint16_t>(t.wj_iz_o), 0xFFF0u - 0x2Au, "1 iz passes through, sign intact");
  check_eq(t.wj_src_id_o, 0x5C00002Au, "1 source id passes through");
  check_eq(t.sj_ready_o, 0, "1 the job is not accepted while the writeback is not ready");
  uint64_t a1 = 0;
  check_eq(take_job(0x2A, &a1), 0x77u, "1 the accepted job carried the grant's ticket");
  check_eq(t.grants_taken_o, 1, "1 one grant consumed");
  check_eq(t.tickets_owed_o, 1, "1 the ticket now owes its FINAL");

  // A base high in the 32-bit HPS space: the sum must CARRY into bit 32 so the
  // writeback can refuse it as unreachable, not wrap into a legal address.
  t.cfg_journal_base_i = 0xFFFFE000u;
  post(3, 0x78u);
  uint64_t a2 = 0;
  take_job(0x2B, &a2);
  check_eq(a2, 0xFFFFE000ull + 3ull * kFBytes, "1 a carrying address keeps its upper half");
  check_eq(a2 >> 32, 1, "1 ...which is nonzero, for the writeback to refuse");
  t.cfg_journal_base_i = kBase;

  // ---- case 2: no grant, no job -------------------------------------------
  reset();
  offer_job(9);
  t.wj_ready_i = 1;
  for (int i = 0; i < 10; ++i) {
    t.eval();
    check_eq(t.wj_valid_o, 0, "2 a job with no grant is NOT offered to the writeback");
    check_eq(t.sj_ready_o, 0, "2 ...and TERRAIN.SEQ is held");
    zhao::tick(t);
  }
  check_eq(t.starved_cycles_o, 10, "2 the wait is counted in cycles");
  check_eq(t.grants_taken_o, 0, "2 nothing was consumed");
  post(1, 0x100u);  // the job is still offered; it goes the cycle the grant lands
  t.eval();
  check_eq(t.wj_valid_o, 1, "2 the grant releases the waiting job");
  check_eq(t.wj_seq_o, 0x100u, "2 ...with that grant's ticket");
  zhao::tick(t);
  quiet();
  t.eval();
  check_eq(t.grants_taken_o, 1, "2 exactly one grant consumed");

  // ---- case 3: order, and a full mailbox ----------------------------------
  reset();
  for (int i = 0; i < kGrants; ++i)
    check_eq(post(10 + i, 1000 + i), 1, "3 a grant is taken while there is room");
  check_eq(t.post_ready_o, 0, "3 a full mailbox refuses the next grant");
  check_eq(post(99, 9999), 0, "3 ...and does not take it");
  check_eq(t.grants_posted_o, kGrants, "3 posted counts only what was taken");
  for (int i = 0; i < kGrants; ++i) {
    uint64_t a = 0;
    check_eq(take_job(0x40 + i, &a), 1000u + i, "3 grants are consumed in posting order");
    check_eq(a, uint64_t{kBase} + uint64_t(10 + i) * kFBytes, "3 ...each with its own slot");
  }

  // ---- case 4: the return stream ------------------------------------------
  // Jobs 1000..1003 are out (case 3). 1000 lands, then completes after its ACK;
  // 1001 is refused before landing; 1002 lands; 1003 lands the SAME CYCLE that
  // 1002 completes.
  landed(1000);
  done(0x40, 1000, true, 0);
  done(0x41, 1001, false, 3);  // kSheetJournal: refused, never landed
  landed(1002);
  // simultaneous
  t.landed_valid_i = 1;
  t.landed_seq_i = 1003;
  t.done_valid_i = 1;
  t.done_slot_i = 0x42;
  t.done_seq_i = 1002;
  t.done_ok_i = 0;
  t.done_verdict_i = 11;  // kSheetNak: landed, and the journal refused it
  t.eval();
  zhao::tick(t);
  quiet();
  t.eval();
  check_eq(t.returns_landed_o, 3, "4 three sheets landed");
  check_eq(t.returns_final_o, 3, "4 three jobs completed");
  check_eq(t.ret_overflow_o, 0, "4 nothing overflowed");
  const Ret e[] = {{1000, 0, 1, 0}, {1000, 1, 1, 0}, {1001, 1, 0, 3},
                   {1002, 0, 1, 0}, {1003, 0, 1, 0}, {1002, 1, 0, 11}};
  for (const Ret& x : e) {
    const Ret r = pop();
    check_eq(r.ticket, x.ticket, "4 return ticket, in arrival order (LANDED before FINAL in a cycle)");
    check_eq(r.final_, x.final_, "4 return kind");
    check_eq(r.ok, x.ok, "4 return ok");
    check_eq(r.verdict, x.verdict, "4 return verdict");
  }
  check_eq(t.ret_valid_o, 0, "4 and nothing else was returned");
  check_eq(t.tickets_owed_o, 1, "4 only 1003 still owes its FINAL");

  // ---- case 5: exactly one FINAL per consumed grant, over many -------------
  reset();
  {
    int finals = 0, landeds = 0;
    for (int n = 0; n < 40; ++n) {
      post(n & 63, 5000 + n);
      const uint32_t tk = take_job(n & 0x3FF, nullptr);
      check_eq(tk, 5000u + n, "5 the job carries its grant's ticket");
      if (n % 3 != 0) landed(tk);
      done(n & 0x3FF, tk, n % 3 != 0, n % 3 != 0 ? 0 : 4);
      for (Ret r = pop(); r.ticket != 0xFFFFFFFFu; r = pop()) {
        check_eq(r.ticket, tk, "5 returns name the job's ticket");
        if (r.final_) ++finals; else ++landeds;
      }
    }
    check_eq(finals, 40, "5 every consumed grant came back FINAL exactly once");
    check_eq(landeds, 26, "5 and LANDED exactly once per sheet that landed");
    check_eq(t.tickets_owed_o, 0, "5 no credit is owed at the end");
  }

  // ---- case 6: the credit holds the queue safe ------------------------------
  reset();
  for (int i = 0; i < kGrants; ++i) post(i, 7000 + i);
  for (int i = 0; i < kTickets; ++i) take_job(i, nullptr);
  post(9, 7009);
  offer_job(9);
  t.wj_ready_i = 1;
  t.eval();
  check_eq(t.wj_valid_o, 0, "6 with TICKETS owed, a job waits even though a grant is posted");
  zhao::tick(t);
  zhao::tick(t);
  quiet();
  t.eval();
  check_eq(t.credit_stall_cycles_o, 2, "6 the credit wait is counted in cycles");
  // Fill the return queue to its brim with the owed tickets' two records each.
  for (int i = 0; i < kTickets; ++i) landed(7000 + i);
  for (int i = 0; i < kTickets; ++i) done(i, 7000 + i, true, 0);
  check_eq(t.ret_overflow_o, 0, "6 2*TICKETS records fit exactly (the guard stays silent)");
  // A LANDED pop returns no credit; a FINAL pop does.
  Ret r = pop();
  check_eq(r.final_, 0, "6 the head is a LANDED");
  offer_job(9);
  t.wj_ready_i = 1;
  t.eval();
  check_eq(t.wj_valid_o, 0, "6 popping a LANDED gives back no credit");
  t.sj_valid_i = 0;
  t.wj_ready_i = 0;
  for (int k = 0; k < 3; ++k) pop();  // three more LANDED
  r = pop();
  check_eq(r.final_, 1, "6 then a FINAL");
  offer_job(9);
  t.wj_ready_i = 1;
  t.eval();
  check_eq(t.wj_valid_o, 1, "6 popping a FINAL gives back the ticket's credit");
  check_eq(t.wj_seq_o, 7009u, "6 ...and the waiting grant goes out");

  if (failures == 0) {
    std::printf("PASS jdoorbell_directed -- %d checks: address law, no-grant wait, order, "
                "LANDED/FINAL per ticket, credit\n", checks);
    zhao::exit_hard(0);
  }
  std::printf("FAILED jdoorbell_directed -- %d of %d checks\n", failures, checks);
  zhao::exit_hard(1);
}
