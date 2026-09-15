// engine1_raw_last_v2_directed.cpp -- Packet-H ENGINE1 raw16 LAST leaf.
//
// Healthy mode proves the three legal byte lengths, physical LAST timing,
// denial/retirement separation, reset-only fail-stop, and the state aliases.
// Inverse builds select one committed RTL mutant and invert the relevant LAST
// assertion so the positive control passes only when that one defect is seen.
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

struct RawObservation {
  bool last;
  bool fault;
  unsigned state;
  bool idle;
  bool pending;
  bool armed;
  unsigned expected;
  unsigned retired;
};

void drive_quiet(Dut& d) {
  d.guard_accept_i = 0;
  d.len_bytes_i = 0;
  d.verdict_ok_i = 0;
  d.verdict_denied_i = 0;
  d.raw16_valid_i = 0;
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

RawObservation sample_raw(Dut& d, bool valid) {
  d.raw16_valid_i = valid ? 1 : 0;
  d.eval();
  RawObservation observation{
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
  d.eval();
  return observation;
}

void check_clean_idle(Dut& d, const char* what) {
  zhao::check(d.state_o == 0 && d.idle_o && !d.pending_verdict_o &&
                  !d.armed_o && d.expected_halfwords_o == 0 &&
                  d.retired_halfwords_o == 0,
              what, 1,
              d.state_o == 0 && d.idle_o && !d.pending_verdict_o &&
                  !d.armed_o && d.expected_halfwords_o == 0 &&
                  d.retired_halfwords_o == 0);
}

void check_healthy_length(unsigned len_bytes, unsigned halfwords) {
  Dut d;
  reset(d);
  zhao::check(d.idle_o && !d.pending_verdict_o && !d.armed_o &&
                  d.expected_halfwords_o == 0 &&
                  d.retired_halfwords_o == 0 && !d.structural_fault_o,
              "reset exposes clean idle aliases and zero counters", 1,
              d.idle_o && !d.pending_verdict_o && !d.armed_o);

  accept(d, len_bytes);
  deliver_verdict(d, true);
  zhao::check(d.armed_o && !d.idle_o && !d.pending_verdict_o &&
                  d.state_o == 2 &&
                  d.expected_halfwords_o == halfwords &&
                  d.retired_halfwords_o == 0 && !d.structural_fault_o,
              "approved legal request arms the raw return", 1,
              d.armed_o && d.state_o == 2);

  // LAST is a physical raw beat sideband, never a free-running terminal flag.
  d.raw16_valid_i = 0;
  d.eval();
  zhao::check(d.raw_last_o == 0,
              "LAST stays low while raw valid is low", 0, d.raw_last_o);

  for (unsigned beat = 1; beat <= halfwords; ++beat) {
    const RawObservation o = sample_raw(d, true);
    const bool terminal = beat == halfwords;
    zhao::check(o.state == 2 && o.armed && !o.idle && !o.pending &&
                    o.expected == halfwords && o.retired == beat - 1,
                "each valid raw beat starts armed with the prior retirement count",
                1, o.armed && o.expected == halfwords &&
                       o.retired == beat - 1);
    zhao::check(o.last == terminal,
                "healthy LAST occurs on the exact terminal raw beat", terminal,
                o.last);
    zhao::check(!o.fault, "healthy raw return remains free of structural fault",
                0, o.fault);

    if (terminal) {
      check_clean_idle(d,
                       "healthy terminal returns idle and clears expected/retired");
    } else {
      d.raw16_valid_i = 0;
      d.eval();
      zhao::check(d.raw_last_o == 0,
                  "inter-beat cycle with raw valid low has no LAST", 0,
                  d.raw_last_o);
      zhao::tick(d);
      d.eval();
      zhao::check(d.armed_o && d.expected_halfwords_o == halfwords &&
                      d.retired_halfwords_o == beat,
                  "nonterminal idle gap preserves the armed count", 1,
                  d.armed_o && d.expected_halfwords_o == halfwords &&
                      d.retired_halfwords_o == beat);
    }
  }

  zhao::check(d.structural_fault_o == 0,
              "complete healthy return does not set structural fault", 0,
              d.structural_fault_o);
}

void test_healthy_terminal_clears_legality() {
  Dut d;
  reset(d);
  accept(d, 16);
  deliver_verdict(d, true);
  for (unsigned beat = 0; beat < 8; ++beat) (void)sample_raw(d, true);
  check_clean_idle(d,
                   "terminal approved return clears the complete transaction");

  // 17 bytes captures an 8-halfword projection but is not one of the three
  // legal byte lengths.  If length_legal_q leaked across the terminal edge,
  // this approval would incorrectly arm the leaf.
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

  // A denied transaction has no armed return owner.  This checks the output
  // side directly without turning the separate raw-after-denial detector into
  // the subject of this denial test.
  d.raw16_valid_i = 1;
  d.eval();
  zhao::check(d.raw_last_o == 0 && d.retired_halfwords_o == 0 &&
                  !d.armed_o,
              "denial emits no LAST and retires zero raw halfwords", 1,
              d.raw_last_o == 0 && d.retired_halfwords_o == 0 && !d.armed_o);
  d.raw16_valid_i = 0;
  d.eval();
}

void test_reset_lifetime_fail_stop_and_recovery() {
  Dut d;
  reset(d);

  // Raw valid in idle is a deliberate protocol violation.  It must latch the
  // fault, and the fault must not be cleared by later apparently-good inputs.
  d.raw16_valid_i = 1;
  d.eval();
  zhao::check(d.raw_last_o == 0,
              "raw valid without an armed owner cannot publish LAST", 0,
              d.raw_last_o);
  zhao::tick(d);
  d.raw16_valid_i = 0;
  d.eval();
  zhao::check(d.structural_fault_o == 1 && d.idle_o &&
                  d.expected_halfwords_o == 0 && d.retired_halfwords_o == 0,
              "protocol fault enters reset-lifetime fail-stop", 1,
              d.structural_fault_o && d.idle_o);

  d.guard_accept_i = 1;
  d.len_bytes_i = 16;
  zhao::tick(d);
  d.guard_accept_i = 0;
  d.eval();
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
#if defined(EXPECT_ENGINE1_RAW_LAST_V2_LATE_LAST_MUTANT)
  constexpr unsigned kRecoveryBeats = 9;
#else
  constexpr unsigned kRecoveryBeats = 8;
#endif
  for (unsigned beat = 0; beat < kRecoveryBeats; ++beat)
    (void)sample_raw(d, true);
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
  d.guard_accept_i = 0;
  d.eval();
  zhao::check(d.structural_fault_o && d.idle_o,
              "overlapping accepted request enters fail-stop", 1,
              d.structural_fault_o && d.idle_o);

  reset(d);
  d.verdict_ok_i = 1;
  zhao::tick(d);
  d.verdict_ok_i = 0;
  d.eval();
  zhao::check(d.structural_fault_o && d.idle_o,
              "verdict without pending request enters fail-stop", 1,
              d.structural_fault_o && d.idle_o);

  reset(d);
  accept(d, 16);
  d.verdict_ok_i = 1;
  d.verdict_denied_i = 1;
  zhao::tick(d);
  d.verdict_ok_i = 0;
  d.verdict_denied_i = 0;
  d.eval();
  zhao::check(d.structural_fault_o && d.idle_o,
              "simultaneous OK and denial enters fail-stop", 1,
              d.structural_fault_o && d.idle_o);

  reset(d);
  accept(d, 16);
  d.raw16_valid_i = 1;
  zhao::tick(d);
  d.raw16_valid_i = 0;
  d.eval();
  zhao::check(d.structural_fault_o && d.idle_o,
              "raw return before approval enters fail-stop", 1,
              d.structural_fault_o && d.idle_o);

  reset(d);
  accept(d, 16);
  deliver_verdict(d, true);
  for (unsigned beat = 0; beat < 8; ++beat) (void)sample_raw(d, true);
  d.raw16_valid_i = 1;
  zhao::tick(d);
  d.raw16_valid_i = 0;
  d.eval();
  zhao::check(d.structural_fault_o && d.idle_o,
              "raw return after terminal enters fail-stop", 1,
              d.structural_fault_o && d.idle_o);
}

#if defined(EXPECT_ENGINE1_RAW_LAST_V2_EARLY_LAST_MUTANT)
void test_early_last_mutant() {
  Dut d;
  reset(d);
  accept(d, 16);
  deliver_verdict(d, true);

  unsigned last_count = 0;
  int first_last = -1;
  for (int beat = 1; beat <= 8; ++beat) {
    const RawObservation o = sample_raw(d, true);
    const bool intended = beat == 7;
    if (o.last) {
      ++last_count;
      if (first_last < 0) first_last = beat;
    }
    zhao::check(o.last == intended,
                "EARLY_LAST mutant has exactly one LAST at raw beat 7", intended,
                o.last);
    zhao::check(!o.fault,
                "EARLY_LAST mutant produces no unrelated structural fault", 0,
                o.fault);
  }
  zhao::check(last_count == 1 && first_last == 7,
              "EARLY_LAST mutant diagnostic is unique and one beat early", 1,
              last_count == 1 && first_last == 7);
  check_clean_idle(d,
                   "EARLY_LAST mutant still retires and closes normally at beat 8");
}
#elif defined(EXPECT_ENGINE1_RAW_LAST_V2_MISSING_LAST_MUTANT)
void test_missing_last_mutant() {
  Dut d;
  reset(d);
  accept(d, 16);
  deliver_verdict(d, true);

  unsigned last_count = 0;
  for (int beat = 1; beat <= 8; ++beat) {
    const RawObservation o = sample_raw(d, true);
    if (o.last) ++last_count;
    zhao::check(!o.last,
                "MISSING_LAST mutant has no physical LAST on any raw beat", 0,
                o.last);
    zhao::check(!o.fault,
                "MISSING_LAST mutant produces no unrelated structural fault", 0,
                o.fault);
  }
  zhao::check(last_count == 0,
              "MISSING_LAST mutant diagnostic is exactly one missing sideband", 0,
              last_count);
  check_clean_idle(d,
                   "MISSING_LAST mutant still retires and closes normally");
}
#elif defined(EXPECT_ENGINE1_RAW_LAST_V2_LATE_LAST_MUTANT)
void test_late_last_mutant() {
  Dut d;
  reset(d);
  accept(d, 16);
  deliver_verdict(d, true);

  unsigned last_count = 0;
  int first_last = -1;
  for (int beat = 1; beat <= 9; ++beat) {
    const RawObservation o = sample_raw(d, true);
    const bool intended = beat == 9;
    if (o.last) {
      ++last_count;
      if (first_last < 0) first_last = beat;
    }
    zhao::check(o.last == intended,
                "LATE_LAST mutant has exactly one LAST at raw beat 9", intended,
                o.last);
    zhao::check(!o.fault,
                "LATE_LAST mutant produces no unrelated structural fault", 0,
                o.fault);
  }
  zhao::check(last_count == 1 && first_last == 9,
              "LATE_LAST mutant diagnostic is unique and one beat late", 1,
              last_count == 1 && first_last == 9);
  check_clean_idle(d,
                   "LATE_LAST mutant closes only after its late terminal beat");
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
  test_denial_zero_retirement();
  test_reset_lifetime_fail_stop_and_recovery();
  test_early_last_mutant();
#elif defined(EXPECT_ENGINE1_RAW_LAST_V2_MISSING_LAST_MUTANT)
  std::printf("engine1_raw_last_v2: inverse MISSING_LAST control\n");
  test_denial_zero_retirement();
  test_reset_lifetime_fail_stop_and_recovery();
  test_missing_last_mutant();
#elif defined(EXPECT_ENGINE1_RAW_LAST_V2_LATE_LAST_MUTANT)
  std::printf("engine1_raw_last_v2: inverse LATE_LAST control\n");
  test_denial_zero_retirement();
  test_reset_lifetime_fail_stop_and_recovery();
  test_late_last_mutant();
#elif defined(EXPECT_ENGINE1_RAW_LAST_V2_STATE_INVALID_MUTANT)
  std::printf("engine1_raw_last_v2: inverse STATE_INVALID control\n");
  test_state_invalid_mutant();
#else
  std::printf("engine1_raw_last_v2: healthy contract\n");
  check_healthy_length(16, 8);
  check_healthy_length(32, 16);
  check_healthy_length(64, 32);
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
