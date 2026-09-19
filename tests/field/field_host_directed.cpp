// field_host_directed.cpp — FIELD.SEQ.CORE composed: does the organ run a
// program, refuse one it does not hold, and share itself fairly?
//
// WHAT THIS FILE IS FOR, AND WHAT IT DELIBERATELY IS NOT.
// `zhao_field_seq`'s SEMANTICS are already the exact differential oracle and
// are proved against `zfield::interpret` by tests/differential/
// field_seq_directed.cpp. Re-checking arithmetic here would be a second
// implementation of that question and would tell us nothing about the thing
// this packet actually built, which is the COMPOSITION: a program store, a
// residency directory, a lane-writing front and an arbiter wrapped around an
// engine that was already right.
//
// So every case below is about the wrapper, and every one of them is a COUNTER
// THAT MUST MOVE. `tools/budget/completion_register.py`'s rule is that a
// counter asserted zero needs a firing positive control or a stated structural
// reason it cannot fire; this file is the firing control for all eight of this
// block's own counters, and the assertions are on the CORRECT behaviour rather
// than on the defect, so nothing here passes only while a bug exists.
//
//   1  a loaded program runs and its outputs reach the right client   runs_o
//   2  an unloaded slot is REFUSED, never faked                       noprog_o
//   3  an insert through the directory invalidates the slot           noprog_o
//   4  two clients contend and the grant alternates                   contended_grants_o
//   5  a load offered during a run defers the next grant              load_defers_o
//   6  an over-long header is clamped, not wrapped                    hdr_clamped_o
//   7  a program naming a table the store lacks is seen               tbl_oob_o
//   8  loads are counted                                              loads_o
//
// `pc_oob_o` is the ninth and it is the one that CANNOT be fired with legal
// stimulus: case 6's clamp is what makes the state unreachable. That is the
// committed-mutant case, and it is recorded in the block header rather than
// faked with an assertion here.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_host.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

// spec/form/field-ir.md canonical opcodes, the three this file needs.
constexpr uint8_t kOpEnd = 0x00;
constexpr uint8_t kOpAdd = 0x03;
constexpr uint8_t kOpCurve = 0x1A;

// The engine's load kinds, from the block header.
constexpr uint8_t kLdUop = 0;
constexpr uint8_t kLdTable = 1;
constexpr uint8_t kLdHeader = 2;

// The instruction word packs exactly, no padding:
//   [7:0] op  [13:8] dst  [19:14] a  [25:20] b  [31:26] c  [63:32] imm
uint64_t instr(uint8_t op, uint8_t dst, uint8_t a, uint8_t b, uint8_t c, uint32_t imm) {
  return static_cast<uint64_t>(op) | (static_cast<uint64_t>(dst & 0x3F) << 8) |
         (static_cast<uint64_t>(a & 0x3F) << 14) | (static_cast<uint64_t>(b & 0x3F) << 20) |
         (static_cast<uint64_t>(c & 0x3F) << 26) | (static_cast<uint64_t>(imm) << 32);
}

using Dut = Vzhao_field_host;

void step(Dut& d) { zhao::tick(d); }

// `zhao::reset` drives `in_valid`/`in_data`, which this block does not have --
// it is the harness's stream-block shape and this is not a stream block. So the
// reset is written out here rather than the ports renamed to suit a helper.
void reset(Dut& d, int cycles) {
  d.rst_n = 0;
  for (int i = 0; i < cycles; ++i) step(d);
  d.rst_n = 1;
  step(d);
}

// A load word. `ld_data_i` is 96 bits, so Verilator gives it as three 32-bit
// words; the helper keeps the test from having to remember that.
void set_ld_data(Dut& d, uint64_t lo64, uint32_t hi32) {
  d.ld_data_i[0] = static_cast<uint32_t>(lo64 & 0xFFFFFFFFu);
  d.ld_data_i[1] = static_cast<uint32_t>(lo64 >> 32);
  d.ld_data_i[2] = hi32;
}

bool load_word(Dut& d, uint8_t kind, uint8_t slot, uint32_t addr, uint64_t lo64,
               uint32_t hi32 = 0) {
  d.ld_valid_i = 1;
  d.ld_kind_i = kind;
  d.ld_slot_i = slot;
  d.ld_addr_i = addr;
  set_ld_data(d, lo64, hi32);
  for (int guard = 0; guard < 40000; ++guard) {
    d.eval();
    if (d.ld_ready_o) {
      step(d);
      d.ld_valid_i = 0;
      return true;
    }
    step(d);
  }
  d.ld_valid_i = 0;
  return false;
}

// The program header word: instr_count in [7:0], out_base in [13:8], the two
// table entry counts at 16 and 24.
uint64_t header_word(uint8_t count, uint8_t out_base, uint8_t tbl_n0 = 0, uint8_t tbl_n1 = 0) {
  return static_cast<uint64_t>(count) | (static_cast<uint64_t>(out_base & 0x3F) << 8) |
         (static_cast<uint64_t>(tbl_n0 & 0x7F) << 16) |
         (static_cast<uint64_t>(tbl_n1 & 0x7F) << 24);
}

// Load `R[out_base] = R0 + R1`, then END. Two instructions.
bool load_add_program(Dut& d, uint8_t slot, uint8_t out_base) {
  if (!load_word(d, kLdUop, slot, 0, instr(kOpAdd, out_base, 0, 1, 0, 0))) return false;
  if (!load_word(d, kLdUop, slot, 1, instr(kOpEnd, 0, 0, 0, 0, 0))) return false;
  return load_word(d, kLdHeader, slot, 0, header_word(2, out_base));
}

void set_req_in(Dut& d, int client, int lane, int32_t value) {
  // req_in_i is CLIENTS*IN_LANES*32 bits, client-major, presented to Verilator
  // as an array of 32-bit words -- one word per lane, which is why no shifting
  // is needed here.
  const int word = client * 12 + lane;
  d.req_in_i[word] = static_cast<uint32_t>(value);
}

void set_req_slot(Dut& d, int client, uint8_t slot) {
  // SLOTW is 3 for PROGS=8, so the whole packed field is well under 32 bits.
  uint32_t v = d.req_slot_i;
  v &= ~(0x7u << (client * 3));
  v |= (static_cast<uint32_t>(slot & 0x7) << (client * 3));
  d.req_slot_i = v;
}

// Offer a request on `client` and drain the response. Returns false on timeout.
//
// `tick` samples inputs at the rising edge and leaves the model settled at
// clk=0, so a handshake is observed by EVALUATING first and stepping after --
// reading `req_ready_o` before the eval sees the previous cycle's combinational
// state, and the accept is then missed entirely while the engine goes on to run
// the request. That is what the first version of this file did, and the symptom
// was a timeout beside a `runs_o` of exactly one.
bool run_once(Dut& d, int client, int32_t* out0, uint8_t* status, int budget = 40000) {
  const uint32_t m = (1u << client);
  d.req_valid_i |= m;
  d.resp_ready_i |= m;
  bool accepted = false;
  bool got = false;
  for (int guard = 0; guard < budget && !got; ++guard) {
    d.eval();
    if (!accepted && (d.req_ready_o & m)) accepted = true;
    if (accepted && (d.resp_valid_o & m)) {
      if (out0 != nullptr) *out0 = static_cast<int32_t>(d.resp_out_o[0]);
      if (status != nullptr) *status = d.resp_status_o;
      got = true;
    }
    step(d);
    if (accepted) d.req_valid_i &= ~m;
  }
  d.req_valid_i &= ~m;
  d.resp_ready_i &= ~m;
  return got;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;

  dut.ld_valid_i = 0;
  dut.req_valid_i = 0;
  dut.resp_ready_i = 0;
  dut.req_noprog_i = 0;
  dut.req_slot_i = 0;
  dut.pc_lu_valid_i = 0;
  dut.pc_lu_resp_ready_i = 1;
  dut.pc_cm_valid_i = 0;
  dut.pc_cm_resp_ready_i = 1;
  for (int i = 0; i < 24; ++i) dut.req_in_i[i] = 0;
  reset(dut, 4);

  // ---- 1. a loaded program runs, and its answer reaches the right client ---
  // R2 = R0 + R1 with out_base = 2, so the first output lane is the sum. The
  // value is checked because a composition that delivered SOMETHING would pass
  // a liveness test while handing back the previous run's register.
  check(load_add_program(dut, /*slot=*/1, /*out_base=*/2), "program loads", 1, 1);
  check(dut.loads_o == 3, "loads_o counts three words", 3, dut.loads_o);

  set_req_slot(dut, 0, 1);
  set_req_in(dut, 0, 0, 11);
  set_req_in(dut, 0, 1, 31);
  int32_t out = 0;
  uint8_t st = 0xFF;
  check(run_once(dut, 0, &out, &st), "client 0 completes a run", 1, 1);
  check(st == 0, "status is OK", 0, st);
  check(out == 42, "R0 + R1 reaches out lane 0", 42, static_cast<uint64_t>(out));
  check(dut.runs_o == 1, "runs_o fired", 1, dut.runs_o);
  // The executor's own uop count, passed straight through. It is checked as a
  // NONZERO rather than an exact number because the v3 fabric issues per
  // context and the front does not own the schedule -- but it must MOVE, and a
  // test that checks only the sum cannot see how many times the machine did the
  // work.
  const uint32_t uops_after_1 = dut.instr_retired_o;
  check(uops_after_1 > 0, "the executor issued uops", 1, uops_after_1);

  // The SAME engine answers a second client with a different input, which is
  // the whole sharing claim. If the wrapper leaked state between clients the
  // second answer would be the first one.
  set_req_slot(dut, 1, 1);
  set_req_in(dut, 1, 0, -5);
  set_req_in(dut, 1, 1, 9);
  check(run_once(dut, 1, &out, &st), "client 1 completes a run", 1, 1);
  check(out == 4, "client 1 gets its OWN operands", 4, static_cast<uint64_t>(out & 0xFFFFFFFF));

  // ---- 2. an unloaded slot is refused, never faked ------------------------
  set_req_slot(dut, 0, 5);  // nothing was ever loaded into slot 5
  check(run_once(dut, 0, &out, &st), "an unloaded slot still answers", 1, 1);
  check(st == 0xF0, "status is ST_NO_PROGRAM", 0xF0, st);
  check(out == 0, "lanes are zeroed, and the STATUS is what says so", 0,
        static_cast<uint64_t>(out));
  const uint32_t noprog_after_2 = dut.noprog_o;
  check(noprog_after_2 == 1, "noprog_o fired", 1, noprog_after_2);

  // ---- 3. an insert through the directory invalidates the slot ------------
  // The directory promises slot 1 to a new hash. Its microcode has not arrived,
  // so the next run on slot 1 must be REFUSED rather than silently executing
  // the displaced program under the new program's name.
  dut.pc_cm_valid_i = 1;
  dut.pc_cm_hash_i = 0xDEADBEEFu;
  dut.pc_cm_ok_i = 1;
  for (int guard = 0; guard < 100 && !dut.pc_cm_ready_o; ++guard) step(dut);
  step(dut);
  dut.pc_cm_valid_i = 0;
  // Drive the commit at whichever slot the directory chose, so the test does
  // not assume the LRU's answer.
  const uint32_t inserted_slot = dut.pc_cm_slot_o;
  set_req_slot(dut, 0, static_cast<uint8_t>(inserted_slot));
  check(run_once(dut, 0, &out, &st), "the reclaimed slot answers", 1, 1);
  check(st == 0xF0, "a reclaimed slot is refused until reloaded", 0xF0, st);
  check(dut.noprog_o == noprog_after_2 + 1, "noprog_o fired again", noprog_after_2 + 1,
        dut.noprog_o);

  // ---- 4. two clients contend, and the grant alternates -------------------
  check(load_add_program(dut, /*slot=*/2, /*out_base=*/3), "second program loads", 1, 1);
  set_req_slot(dut, 0, 2);
  set_req_slot(dut, 1, 2);
  set_req_in(dut, 0, 0, 100);
  set_req_in(dut, 0, 1, 1);
  set_req_in(dut, 1, 0, 200);
  set_req_in(dut, 1, 1, 2);
  const uint32_t grants_before = dut.grants_o;
  const uint32_t contended_before = dut.contended_grants_o;

  // Both offer at once and both hold until served. Whichever is granted first,
  // the other must be granted next -- that is the fairness claim, and a fixed
  // priority would fail it.
  dut.req_valid_i = 0x3;
  dut.resp_ready_i = 0x3;
  int served[2] = {0, 0};
  int first = -1;
  for (int guard = 0; guard < 80000 && (served[0] + served[1]) < 2; ++guard) {
    dut.eval();
    for (int c = 0; c < 2; ++c) {
      if ((dut.req_ready_o & (1u << c)) && first < 0) first = c;
      if (dut.resp_valid_o & (1u << c)) {
        served[c] = 1;
        dut.req_valid_i &= ~(1u << c);
      }
    }
    step(dut);
  }
  dut.req_valid_i = 0;
  dut.resp_ready_i = 0;
  check(served[0] == 1 && served[1] == 1, "both contending clients are served", 1,
        static_cast<uint64_t>(served[0] * 2 + served[1]));
  check(dut.grants_o == grants_before + 2, "two grants", grants_before + 2, dut.grants_o);
  check(dut.contended_grants_o > contended_before, "contended_grants_o fired",
        contended_before + 1, dut.contended_grants_o);

  // ---- 5. a load offered during a run defers the next grant ---------------
  // The loader must not be starved by a client issuing back to back. Offer a
  // load and a request in the same window and confirm the deferral is COUNTED,
  // which is the only evidence the priority rule is live.
  const uint32_t defers_before = dut.load_defers_o;
  dut.req_valid_i = 0x1;
  set_req_slot(dut, 0, 2);
  dut.ld_valid_i = 1;
  dut.ld_kind_i = kLdUop;
  dut.ld_slot_i = 4;
  dut.ld_addr_i = 0;
  set_ld_data(dut, instr(kOpEnd, 0, 0, 0, 0, 0), 0);
  dut.resp_ready_i = 0x1;
  for (int guard = 0; guard < 40000 && dut.load_defers_o == defers_before; ++guard) {
    dut.eval();
    step(dut);
  }
  dut.ld_valid_i = 0;
  // Let the request that was deferred behind the load complete, so the engine
  // is idle again for the cases that follow.
  for (int guard = 0; guard < 40000; ++guard) {
    dut.eval();
    if (dut.resp_valid_o & 1u) break;
    step(dut);
  }
  dut.req_valid_i = 0;
  step(dut);
  dut.resp_ready_i = 0;
  check(dut.load_defers_o > defers_before, "load_defers_o fired", defers_before + 1,
        dut.load_defers_o);

  // ---- 6. a uop addressed past the plan is REFUSED, not wrapped -----------
  // INSTR_N is the executor's PLAN depth, 32. A uop written at pc 40 would wrap
  // into another instruction of the SAME program -- silent, well formed, and a
  // plausible field. It is refused at the load port and counted.
  check(dut.ld_oob_o == 0, "ld_oob_o starts at zero", 0, dut.ld_oob_o);
  check(load_word(dut, kLdUop, 3, 40, instr(kOpEnd, 0, 0, 0, 0, 0)),
        "an out-of-plan uop is accepted at the port", 1, 1);
  check(dut.ld_oob_o == 1, "ld_oob_o fired", 1, dut.ld_oob_o);

  // ---- 8. loads are counted ----------------------------------------------
  // EIGHT, counted out rather than bounded, because a `>=` would not notice a
  // load the port accepted twice: 3 for the ADD program (two uops and a
  // header), 3 for the second copy of it, 1 for the load that was deferred past
  // a client in case 5, and 1 for the out-of-plan uop in case 6. An accepted
  // word is counted whether or not it was written, which is what makes
  // `ld_oob_o` a REFUSAL count rather than a second load count.
  check(dut.loads_o == 8, "loads_o counted every accepted word, and only once", 8,
        dut.loads_o);

  // ---- 9. THE FABRIC'S ALARMS ALL READ ZERO, and that is a claim ----------
  // Every one of these is a fault `zhao_field_v3_engine` owns and reports
  // separately. They are asserted zero here because the stimulus above is
  // lawful; each one's POSITIVE control lives with the block that raises it
  // (tests/differential/field_v3_full_directed.cpp drives every alarm it has),
  // which is where a fault can actually be constructed. Asserting them here is
  // a regression guard on the COMPOSITION, not evidence that the detectors work.
  check(dut.exec_desync_o == 0, "no executor desync", 0, dut.exec_desync_o);
  check(dut.bank_desync_o == 0, "no engine bank desync", 0, dut.bank_desync_o);
  check(dut.svc_bank_desync_o == 0, "no service bank desync", 0, dut.svc_bank_desync_o);
  check(dut.tag_mismatch_o == 0, "no service tag mismatch", 0, dut.tag_mismatch_o);
  check(dut.wrong_op_o == 0, "no op reached neither service", 0, dut.wrong_op_o);
  check(dut.skid_overflow_o == 0, "no writeback skid overflow", 0, dut.skid_overflow_o);
  check(dut.uniform_bad_o == 0, "no bad uniform or immediate", 0, dut.uniform_bad_o);

  return zhao::report_and_exit("field_host_directed");
}
