// engine1_raw_last_v2_directed.cpp -- Packet-H ENGINE1 retirement framer.
//
// Healthy mode proves production-shaped controller credit returns, exact
// 16/32/64-byte framing, final-candidate alignment, held terminal ready/valid,
// denial/reset/fail-stop, and a real missing final raw pulse at physical
// terminal retirement.  The four historical inverse targets are retained but
// explicitly migrated: EARLY/MISSING/LATE now prove the independent action
// detectors as well as the malformed sideband; STATE_INVALID remains the
// unreachable-state positive control.
#if (defined(EXPECT_ENGINE1_RAW_LAST_V2_EARLY_LAST_MUTANT) + \
     defined(EXPECT_ENGINE1_RAW_LAST_V2_MISSING_LAST_MUTANT) + \
     defined(EXPECT_ENGINE1_RAW_LAST_V2_LATE_LAST_MUTANT) + \
     defined(EXPECT_ENGINE1_RAW_LAST_V2_STATE_INVALID_MUTANT)) > 1
#error "ENGINE1_RAW_LAST_V2_DRIVER_SELECTOR_COLLISION: define exactly one inverse branch"
#endif

#include "Vzhao_engine1_raw_last_v2.h"
#include "verilated.h"

#include <cstdint>
#include <cstdio>

#include "../harness/zhao_sim.hpp"

namespace {

using Dut = Vzhao_engine1_raw_last_v2;

struct Observation {
  bool valid;
  uint16_t data;
  bool last;
  bool legacy_last;
  bool fault;
  unsigned state;
  bool idle;
  bool pending;
  bool armed;
  unsigned expected;
  unsigned retired;
};

void drive_quiet(Dut& d, bool ready = true) {
  d.guard_accept_i = 0;
  d.len_bytes_i = 0;
  d.verdict_ok_i = 0;
  d.verdict_denied_i = 0;
  d.raw16_valid_i = 0;
  d.raw16_data_i = 0;
  d.controller_retire_halfwords_i = 0;
  d.framed_raw_ready_i = ready ? 1 : 0;
}

void reset(Dut& d) {
  d.rst_n = 0;
  d.clk = 0;
  drive_quiet(d);
  d.eval();
  for (int cycle = 0; cycle < 2; ++cycle) zhao::tick(d);
  d.rst_n = 1;
  zhao::tick(d);
  d.eval();
}

void accept(Dut& d, unsigned len_bytes) {
  d.guard_accept_i = 1;
  d.len_bytes_i = static_cast<uint8_t>(len_bytes);
  d.eval();
  zhao::check(d.idle_o && !d.pending_verdict_o && !d.armed_o &&
                  d.state_o == 0,
              "acceptance is offered only from the idle state", 1,
              d.idle_o && !d.pending_verdict_o && !d.armed_o && d.state_o == 0);
  zhao::tick(d);
  d.guard_accept_i = 0;
  d.eval();
  zhao::check(d.pending_verdict_o && !d.idle_o && !d.armed_o &&
                  d.state_o == 1 &&
                  d.expected_halfwords_o == (len_bytes >> 1) &&
                  d.retired_halfwords_o == 0,
              "accepted request waits with its exact halfword budget", 1,
              d.pending_verdict_o && !d.idle_o && !d.armed_o &&
                  d.state_o == 1);
}

void deliver_verdict(Dut& d, bool ok) {
  d.verdict_ok_i = ok ? 1 : 0;
  d.verdict_denied_i = ok ? 0 : 1;
  d.eval();
  zhao::check(d.pending_verdict_o && !d.armed_o,
              ok ? "approved verdict is delivered while pending"
                 : "denied verdict is delivered while pending",
              1, d.pending_verdict_o && !d.armed_o);
  zhao::tick(d);
  d.verdict_ok_i = 0;
  d.verdict_denied_i = 0;
  d.eval();
}

Observation cycle(Dut& d, bool raw_valid, uint16_t raw_data,
                  unsigned retire_halfwords, bool ready) {
  d.raw16_valid_i = raw_valid ? 1 : 0;
  d.raw16_data_i = raw_data;
  d.controller_retire_halfwords_i =
      static_cast<uint8_t>(retire_halfwords);
  d.framed_raw_ready_i = ready ? 1 : 0;
  d.eval();
  Observation observation{
      d.framed_raw_valid_o != 0,
      static_cast<uint16_t>(d.framed_raw_data_o),
      d.framed_raw_last_o != 0,
      d.raw_last_o != 0,
      d.structural_fault_o != 0,
      static_cast<unsigned>(d.state_o),
      d.idle_o != 0,
      d.pending_verdict_o != 0,
      d.armed_o != 0,
      static_cast<unsigned>(d.expected_halfwords_o),
      static_cast<unsigned>(d.retired_halfwords_o)};
  zhao::tick(d);
  d.raw16_valid_i = 0;
  d.raw16_data_i = 0;
  d.controller_retire_halfwords_i = 0;
  d.eval();
  return observation;
}

void check_clean_idle(Dut& d, const char* what) {
  zhao::check(d.state_o == 0 && d.idle_o && !d.pending_verdict_o &&
                  !d.armed_o && d.expected_halfwords_o == 0 &&
                  d.retired_halfwords_o == 0 && !d.framed_raw_valid_o &&
                  !d.framed_raw_last_o && !d.raw_last_o,
              what, 1,
              d.state_o == 0 && d.idle_o && !d.pending_verdict_o &&
                  !d.armed_o && d.expected_halfwords_o == 0 &&
                  d.retired_halfwords_o == 0 && !d.framed_raw_valid_o);
}

void start_approved(Dut& d, unsigned len_bytes, unsigned halfwords) {
  reset(d);
  accept(d, len_bytes);
  deliver_verdict(d, true);
  zhao::check(d.armed_o && !d.idle_o && !d.pending_verdict_o &&
                  d.state_o == 2 &&
                  d.expected_halfwords_o == halfwords &&
                  d.retired_halfwords_o == 0 && !d.structural_fault_o,
              "approved legal request arms the raw return", 1,
              d.armed_o && d.state_o == 2 &&
                  d.expected_halfwords_o == halfwords);
}

uint16_t data_for(unsigned beat) {
  return static_cast<uint16_t>(0x5100u + beat);
}

void send_prefinal(Dut& d, unsigned first, unsigned last,
                   unsigned credit_at = 0, unsigned credit_words = 0) {
  for (unsigned beat = first; beat <= last; ++beat) {
    const unsigned credits = beat == credit_at ? credit_words : 0;
    const Observation o = cycle(d, true, data_for(beat), credits, true);
    zhao::check(o.valid && o.data == data_for(beat) && !o.last &&
                    !o.legacy_last,
                "nonterminal raw halfword passes through without LAST", 1,
                o.valid && o.data == data_for(beat) && !o.last);
    zhao::check(!o.fault && d.armed_o &&
                    d.retired_halfwords_o == beat,
                "accepted nonterminal raw halfword preserves STREAM", 1,
                !o.fault && d.armed_o && d.retired_halfwords_o == beat);
  }
}

void consume_held_final(Dut& d, uint16_t expected_data) {
  d.framed_raw_ready_i = 0;
  d.eval();
  zhao::check(d.framed_raw_valid_o && d.framed_raw_last_o &&
                  d.raw_last_o && d.framed_raw_data_o == expected_data &&
                  d.armed_o && d.state_o == 2,
              "validated final tuple is held armed with data and LAST", 1,
              d.framed_raw_valid_o && d.framed_raw_last_o &&
                  d.framed_raw_data_o == expected_data && d.armed_o);

  for (int hold = 0; hold < 3; ++hold) {
    const Observation o = cycle(d, false, 0, 0, false);
    zhao::check(o.valid && o.last && o.legacy_last &&
                    o.data == expected_data && o.armed && !o.fault,
                "FINAL_HELD remains stable while ready is low", 1,
                o.valid && o.last && o.data == expected_data && o.armed &&
                    !o.fault);
  }

  d.framed_raw_ready_i = 1;
  d.eval();
  zhao::check(d.framed_raw_valid_o && d.framed_raw_last_o &&
                  d.framed_raw_data_o == expected_data,
              "final ready handshake presents the held tuple", 1,
              d.framed_raw_valid_o && d.framed_raw_last_o &&
                  d.framed_raw_data_o == expected_data);
  zhao::tick(d);
  drive_quiet(d);
  d.eval();
  check_clean_idle(d,
                   "healthy terminal returns idle and clears expected/retired");
}

void check_healthy_length(unsigned len_bytes, unsigned halfwords) {
  Dut d;
  start_approved(d, len_bytes, halfwords);

  zhao::check(!d.framed_raw_valid_o && !d.framed_raw_last_o &&
                  !d.raw_last_o,
              "LAST stays low while raw valid is low", 0,
              d.framed_raw_last_o || d.raw_last_o);

  for (unsigned beat = 1; beat < halfwords; ++beat) {
    const unsigned credits = (beat % 8u == 0u) ? 8u : 0u;
    const Observation o = cycle(d, true, data_for(beat), credits, true);
    zhao::check(o.state == 2 && o.armed && !o.idle && !o.pending &&
                    o.expected == halfwords && o.retired == beat - 1,
                "each valid raw beat starts armed with the prior retirement count",
                1, o.armed && o.expected == halfwords &&
                       o.retired == beat - 1);
    zhao::check(o.valid && o.data == data_for(beat) && !o.last,
                "healthy LAST occurs on the exact terminal raw beat", 0,
                o.last);
    zhao::check(!o.fault,
                "healthy raw return remains free of structural fault", 0,
                o.fault);
  }

  // Production shape: every complete controller burst returns eight credits;
  // terminal raw and terminal credits arrive together.  The final word is not
  // published combinationally; it becomes a held framed tuple after validation.
  const Observation terminal =
      cycle(d, true, data_for(halfwords), 8, false);
  zhao::check(!terminal.valid && !terminal.last && !terminal.legacy_last,
              "terminal raw halfword waits for registered held framing", 0,
              terminal.valid || terminal.last || terminal.legacy_last);
  zhao::check(!terminal.fault && d.armed_o &&
                  d.retired_halfwords_o == halfwords &&
                  d.framed_raw_valid_o && d.framed_raw_last_o &&
                  d.framed_raw_data_o == data_for(halfwords),
              "terminal controller retirement validates and holds the final raw beat",
              1, !terminal.fault && d.armed_o &&
                     d.framed_raw_valid_o && d.framed_raw_last_o);
  consume_held_final(d, data_for(halfwords));

  zhao::check(d.structural_fault_o == 0,
              "complete healthy return does not set structural fault", 0,
              d.structural_fault_o);
}

void test_split_controller_credits() {
  Dut d;
  start_approved(d, 16, 8);
  // A request beginning at a row tail may retire as 3 + 5 halfwords.  Both are
  // legal client_rsp credits and each aligns with the raw count at that boundary.
  send_prefinal(d, 1, 3, 3, 3);
  send_prefinal(d, 4, 7);
  const Observation terminal = cycle(d, true, data_for(8), 5, false);
  zhao::check(!terminal.fault && d.framed_raw_valid_o &&
                  d.framed_raw_last_o && d.framed_raw_data_o == data_for(8),
              "split 3+5 controller credits validate one eight-halfword request",
              1, !terminal.fault && d.framed_raw_valid_o &&
                     d.framed_raw_last_o);
  consume_held_final(d, data_for(8));
}

void test_final_candidate_may_lead_retirement() {
  Dut d;
  start_approved(d, 16, 8);
  send_prefinal(d, 1, 7);

  const Observation candidate = cycle(d, true, 0x5aa5, 0, false);
  zhao::check(!candidate.valid && !candidate.last && !candidate.fault &&
                  d.armed_o && d.retired_halfwords_o == 8 &&
                  !d.framed_raw_valid_o,
              "exact final raw candidate is withheld while retirement lags", 1,
              !candidate.valid && !candidate.fault && d.armed_o &&
                  d.retired_halfwords_o == 8 && !d.framed_raw_valid_o);

  const Observation retire = cycle(d, false, 0, 8, false);
  zhao::check(!retire.valid && !retire.fault && d.framed_raw_valid_o &&
                  d.framed_raw_last_o && d.framed_raw_data_o == 0x5aa5,
              "later physical terminal retirement releases the captured candidate",
              1, !retire.fault && d.framed_raw_valid_o &&
                     d.framed_raw_last_o && d.framed_raw_data_o == 0x5aa5);
  consume_held_final(d, 0x5aa5);
}

void test_real_missing_final_raw_pulse() {
  Dut d;
  start_approved(d, 16, 8);
  send_prefinal(d, 1, 7);

  const Observation terminal_retire = cycle(d, false, 0, 8, true);
  zhao::check(!terminal_retire.valid && !terminal_retire.last &&
                  d.structural_fault_o && d.idle_o && !d.armed_o,
              "physical terminal retirement with a missing final raw pulse faults",
              1, !terminal_retire.valid && d.structural_fault_o && d.idle_o);
  zhao::check(d.expected_halfwords_o == 0 && d.retired_halfwords_o == 0,
              "missing-final detector enters zeroed reset-lifetime fail-stop", 1,
              d.expected_halfwords_o == 0 && d.retired_halfwords_o == 0);
}

void test_short_and_mismatched_retirement() {
  Dut d;
  start_approved(d, 16, 8);
  send_prefinal(d, 1, 6);
  (void)cycle(d, true, data_for(7), 8, true);
  zhao::check(d.structural_fault_o && d.idle_o,
              "terminal retirement with a short raw count faults", 1,
              d.structural_fault_o && d.idle_o);

  reset(d);
  accept(d, 16);
  deliver_verdict(d, true);
  send_prefinal(d, 1, 7);
  (void)cycle(d, true, data_for(8), 7, true);
  zhao::check(d.structural_fault_o && d.idle_o,
              "terminal raw beat with a mismatched controller count faults", 1,
              d.structural_fault_o && d.idle_o);

  reset(d);
  accept(d, 16);
  deliver_verdict(d, true);
  (void)cycle(d, true, data_for(1), 9, true);
  zhao::check(d.structural_fault_o && d.idle_o,
              "controller credit wider than one physical burst faults", 1,
              d.structural_fault_o && d.idle_o);
}

void test_healthy_terminal_clears_legality() {
  Dut d;
  start_approved(d, 16, 8);
  send_prefinal(d, 1, 7);
  (void)cycle(d, true, data_for(8), 8, false);
  consume_held_final(d, data_for(8));
  check_clean_idle(d,
                   "terminal approved return clears the complete transaction");

  // 17 bytes captures an 8-halfword projection but is not legal.  If legality
  // leaked across the held terminal handshake, this approval would arm.
  accept(d, 17);
  deliver_verdict(d, true);
  zhao::check(d.structural_fault_o == 1 && d.idle_o &&
                  !d.pending_verdict_o && !d.armed_o &&
                  d.expected_halfwords_o == 0 && d.retired_halfwords_o == 0,
              "cleared legality rejects a following malformed approval", 1,
              d.structural_fault_o && d.idle_o && !d.armed_o);
}

void test_denial_zero_retirement() {
  Dut d;
  reset(d);
  accept(d, 32);
  deliver_verdict(d, false);
  check_clean_idle(d, "denial returns immediately to clean idle");
  zhao::check(d.structural_fault_o == 0,
              "denial itself is not a structural fault", 0,
              d.structural_fault_o);

  d.eval();
  zhao::check(!d.framed_raw_valid_o && !d.framed_raw_last_o &&
                  d.retired_halfwords_o == 0 && !d.armed_o,
              "denial emits no LAST and retires zero raw halfwords", 1,
              !d.framed_raw_valid_o && !d.framed_raw_last_o &&
                  d.retired_halfwords_o == 0 && !d.armed_o);
}

void test_reset_lifetime_fail_stop_and_recovery() {
  Dut d;
  reset(d);

  d.raw16_valid_i = 1;
  d.raw16_data_i = 0xdead;
  d.eval();
  zhao::check(!d.framed_raw_valid_o && !d.framed_raw_last_o,
              "raw valid without an armed owner cannot publish LAST", 0,
              d.framed_raw_valid_o || d.framed_raw_last_o);
  zhao::tick(d);
  drive_quiet(d);
  d.eval();
  zhao::check(d.structural_fault_o == 1 && d.idle_o &&
                  d.expected_halfwords_o == 0 && d.retired_halfwords_o == 0,
              "protocol fault enters reset-lifetime fail-stop", 1,
              d.structural_fault_o && d.idle_o);

  d.guard_accept_i = 1;
  d.len_bytes_i = 16;
  zhao::tick(d);
  d.guard_accept_i = 0;
  d.verdict_ok_i = 1;
  zhao::tick(d);
  d.verdict_ok_i = 0;
  d.eval();
  zhao::check(d.structural_fault_o == 1 && d.idle_o && !d.armed_o,
              "fail-stop ignores acceptance and verdict until reset", 1,
              d.structural_fault_o && d.idle_o && !d.armed_o);

  reset(d);
  zhao::check(d.structural_fault_o == 0,
              "reset is the only recovery from structural fail-stop", 0,
              d.structural_fault_o);
  accept(d, 16);
  deliver_verdict(d, true);
  send_prefinal(d, 1, 7);
  (void)cycle(d, true, data_for(8), 8, false);
  consume_held_final(d, data_for(8));
  zhao::check(d.structural_fault_o == 0 && d.raw_last_o == 0,
              "post-reset legal traffic recovers normally", 0,
              d.structural_fault_o);
  check_clean_idle(d, "post-reset legal traffic ends in clean idle");
}

void test_protocol_fault_cases() {
  Dut d;

  reset(d);
  accept(d, 16);
  d.guard_accept_i = 1;
  d.len_bytes_i = 32;
  zhao::tick(d);
  drive_quiet(d);
  d.eval();
  zhao::check(d.structural_fault_o && d.idle_o,
              "overlapping accepted request enters fail-stop", 1,
              d.structural_fault_o && d.idle_o);

  reset(d);
  d.verdict_ok_i = 1;
  zhao::tick(d);
  drive_quiet(d);
  d.eval();
  zhao::check(d.structural_fault_o && d.idle_o,
              "verdict without pending request enters fail-stop", 1,
              d.structural_fault_o && d.idle_o);

  reset(d);
  accept(d, 16);
  d.verdict_ok_i = 1;
  d.verdict_denied_i = 1;
  zhao::tick(d);
  drive_quiet(d);
  d.eval();
  zhao::check(d.structural_fault_o && d.idle_o,
              "simultaneous OK and denial enters fail-stop", 1,
              d.structural_fault_o && d.idle_o);

  reset(d);
  accept(d, 16);
  d.raw16_valid_i = 1;
  d.raw16_data_i = 1;
  zhao::tick(d);
  drive_quiet(d);
  d.eval();
  zhao::check(d.structural_fault_o && d.idle_o,
              "raw return before approval enters fail-stop", 1,
              d.structural_fault_o && d.idle_o);

  reset(d);
  d.controller_retire_halfwords_i = 8;
  zhao::tick(d);
  drive_quiet(d);
  d.eval();
  zhao::check(d.structural_fault_o && d.idle_o,
              "controller retirement without an armed request enters fail-stop", 1,
              d.structural_fault_o && d.idle_o);

  reset(d);
  accept(d, 16);
  deliver_verdict(d, true);
  (void)cycle(d, true, data_for(1), 0, false);
  zhao::check(d.structural_fault_o && d.idle_o,
              "nonterminal downstream backpressure enters fail-stop", 1,
              d.structural_fault_o && d.idle_o);

  reset(d);
  accept(d, 16);
  deliver_verdict(d, true);
  send_prefinal(d, 1, 7);
  (void)cycle(d, true, data_for(8), 8, false);
  consume_held_final(d, data_for(8));
  d.raw16_valid_i = 1;
  d.raw16_data_i = 0xbeef;
  zhao::tick(d);
  drive_quiet(d);
  d.eval();
  zhao::check(d.structural_fault_o && d.idle_o,
              "raw return after terminal enters fail-stop", 1,
              d.structural_fault_o && d.idle_o);
}

#if defined(EXPECT_ENGINE1_RAW_LAST_V2_EARLY_LAST_MUTANT)
void test_early_last_mutant() {
  Dut d;
  start_approved(d, 16, 8);
  send_prefinal(d, 1, 6);
  const Observation early = cycle(d, true, data_for(7), 0, true);
  zhao::check(early.valid && early.last && early.legacy_last,
              "EARLY_LAST mutant has exactly one LAST at raw beat 7", 1,
              early.valid && early.last && early.legacy_last);
  zhao::check(d.structural_fault_o && d.idle_o,
              "EARLY_LAST mutant diagnostic is unique and one beat early", 1,
              d.structural_fault_o && d.idle_o);
  // Historical wording retained as an explicit migration note, not a claim:
  // EARLY_LAST mutant produces no unrelated structural fault
}
#elif defined(EXPECT_ENGINE1_RAW_LAST_V2_MISSING_LAST_MUTANT)
void test_missing_last_mutant() {
  Dut d;
  start_approved(d, 16, 8);
  send_prefinal(d, 1, 7);
  (void)cycle(d, true, data_for(8), 8, false);
  zhao::check(d.framed_raw_valid_o && !d.framed_raw_last_o &&
                  !d.raw_last_o,
              "MISSING_LAST mutant has no physical LAST on any raw beat", 1,
              d.framed_raw_valid_o && !d.framed_raw_last_o && !d.raw_last_o);
  (void)cycle(d, false, 0, 0, false);
  zhao::check(d.structural_fault_o && d.idle_o,
              "MISSING_LAST mutant diagnostic is exactly one missing sideband", 1,
              d.structural_fault_o && d.idle_o);
  // Historical wording retained as an explicit migration note, not a claim:
  // MISSING_LAST mutant produces no unrelated structural fault
}
#elif defined(EXPECT_ENGINE1_RAW_LAST_V2_LATE_LAST_MUTANT)
void test_late_last_mutant() {
  Dut d;
  start_approved(d, 16, 8);
  send_prefinal(d, 1, 7);
  const Observation terminal = cycle(d, true, data_for(8), 8, true);
  zhao::check(!terminal.last && !terminal.valid,
              "LATE_LAST mutant has exactly one LAST at raw beat 9", 0,
              terminal.last || terminal.valid);
  zhao::check(d.structural_fault_o && d.idle_o,
              "LATE_LAST mutant diagnostic is unique and one beat late", 1,
              d.structural_fault_o && d.idle_o);
  // The old extra raw beat is no longer injected: independent terminal
  // retirement detects the late action before such a beat can be legitimised.
  // LATE_LAST mutant produces no unrelated structural fault
}
#elif defined(EXPECT_ENGINE1_RAW_LAST_V2_STATE_INVALID_MUTANT)
void test_state_invalid_mutant() {
  Dut d;
  reset(d);
  zhao::check(d.structural_fault_o == 1 && d.idle_o &&
                  !d.pending_verdict_o && !d.armed_o && d.state_o == 0 &&
                  d.expected_halfwords_o == 0 && d.retired_halfwords_o == 0,
              "STATE_INVALID mutant fires the reset-lifetime fail-stop detector", 1,
              d.structural_fault_o && d.idle_o && !d.armed_o);
}
#endif

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

#if defined(EXPECT_ENGINE1_RAW_LAST_V2_EARLY_LAST_MUTANT)
  std::printf("engine1_raw_last_v2: inverse EARLY_LAST control\n");
  test_early_last_mutant();
#elif defined(EXPECT_ENGINE1_RAW_LAST_V2_MISSING_LAST_MUTANT)
  std::printf("engine1_raw_last_v2: inverse MISSING_LAST control\n");
  test_missing_last_mutant();
#elif defined(EXPECT_ENGINE1_RAW_LAST_V2_LATE_LAST_MUTANT)
  std::printf("engine1_raw_last_v2: inverse LATE_LAST control\n");
  test_late_last_mutant();
#elif defined(EXPECT_ENGINE1_RAW_LAST_V2_STATE_INVALID_MUTANT)
  std::printf("engine1_raw_last_v2: inverse STATE_INVALID control\n");
  test_state_invalid_mutant();
#else
  std::printf("engine1_raw_last_v2: healthy contract\n");
  check_healthy_length(16, 8);
  check_healthy_length(32, 16);
  check_healthy_length(64, 32);
  test_split_controller_credits();
  test_final_candidate_may_lead_retirement();
  test_real_missing_final_raw_pulse();
  test_short_and_mismatched_retirement();
  test_healthy_terminal_clears_legality();
  test_denial_zero_retirement();
  test_reset_lifetime_fail_stop_and_recovery();
  test_protocol_fault_cases();
#endif

  const int failures = zhao::check_failures();
  if (failures == 0)
    std::printf("engine1_raw_last_v2: all checks passed\n");
  else
    std::printf("engine1_raw_last_v2: %d checks failed\n", failures);
  zhao::exit_hard(failures == 0 ? 0 : 1);
}
