// field_gather_replicate_mutant.cpp -- THE GATHER'S DISCRIMINATOR, SEEN TO
// FAIL. POLARITY INVERTED: THIS TEST PASSES WHEN THE FOUR ANSWERS COLLAPSE.
//
// ===========================================================================
// WHAT THIS IS EVIDENCE ABOUT
// ===========================================================================
// `field_gather_front_census` asserts that a four-point group's four points
// answer four DIFFERENT values, and everything else it checks passes just as
// well on a front that evaluates point 0 four times: the cadence, the group
// count, `StOk`, the derived overlap, `resp_count_o`, and "the output is 42".
// There is NO legal stimulus that tells the two fronts apart, because the
// difference is not at the interface -- it is in which point's registers
// reached which fabric lane.
//
// `CLAUDE.md`: "A guard you cannot reach with legal stimulus needs a COMMITTED
// MUTANT ... with a driver whose polarity is inverted -- it passes when the
// counter FIRES. It is evidence about the instrument, not about the design."
//
// So `tests/mutants/zhao_field_host_v2_gather_replicate_mutant.sv` is the
// production host with ONE LINE changed -- `cur_in[0][lane_sel]` into every
// fabric lane instead of `cur_in[pl][lane_sel]` -- and this file drives it and
// REQUIRES the collapse. If this test ever goes red, the census's
// discriminator has stopped discriminating and its green is worth nothing.
//
// ===========================================================================
// AND THIS IS NOT "A TEST THAT ASSERTS THE BUG"
// ===========================================================================
// That rule -- `CLAUDE.md`, "do not write a test that asserts the bug" -- is
// about asserting a defect IN PRODUCTION, where the assertion has to be
// deleted once the defect is repaired. Nothing here is production. The
// correct behaviour is asserted in `field_gather_front_census`, against the
// shipped module; this asserts that a DELIBERATELY BROKEN COPY behaves the
// broken way, which is the only form in which "the check can fail" is a
// measurement rather than an argument.
//
// It also happens to be a faithful model of the front the console composes
// today, which is the second thing worth having it for.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_field_host_v2_gather_replicate_mutant.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

constexpr uint8_t kOpEnd = 0x00;
constexpr uint8_t kOpAdd = 0x03;

constexpr uint8_t kLdUop = 0;
constexpr uint8_t kLdHeader = 2;
constexpr uint8_t kLdOutMap = 4;
constexpr uint8_t kLdAssoc = 5;

constexpr uint8_t kSrcVectorReg = 0;
constexpr uint8_t kFormCanonical = 0;
constexpr uint8_t kStOk = 0x00;

constexpr int kClients = 4;
constexpr int kOrdinals = 7;
constexpr int kInLanes = 15;
constexpr int kFrontPts = 4;

using Dut = Vzhao_field_host_v2_gather_replicate_mutant;

void step(Dut& d) { zhao::tick(d); }

void reset(Dut& d, int cycles) {
  d.rst_n = 0;
  for (int i = 0; i < cycles; ++i) step(d);
  d.rst_n = 1;
  step(d);
}

uint64_t instr(uint8_t op, uint8_t dst, uint8_t a, uint8_t b, uint8_t c, uint32_t imm) {
  return static_cast<uint64_t>(op) | (static_cast<uint64_t>(dst & 0x3F) << 8) |
         (static_cast<uint64_t>(a & 0x3F) << 14) | (static_cast<uint64_t>(b & 0x3F) << 20) |
         (static_cast<uint64_t>(c & 0x3F) << 26) | (static_cast<uint64_t>(imm) << 32);
}

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

uint64_t header_word(uint8_t instr_count, uint8_t out_base, uint8_t win_mask, uint8_t ord_mask,
                     uint8_t out_count, uint8_t form) {
  uint64_t w = static_cast<uint64_t>(instr_count) | (static_cast<uint64_t>(out_base & 0x3F) << 8);
  w |= (static_cast<uint64_t>(win_mask) & 0x7Full) << 32;
  w |= (static_cast<uint64_t>(ord_mask) & 0xFFull) << 48;
  w |= (static_cast<uint64_t>(out_count) & 0x0Full) << 56;
  w |= (static_cast<uint64_t>(form) & 0x03ull) << 60;
  return w;
}

void set_req_in(Dut& d, int client, int pt, int lane, int32_t value) {
  d.req_in_i[(client * kFrontPts + pt) * kInLanes + lane] = static_cast<uint32_t>(value);
}

int32_t get_resp_out(Dut& d, int pt, int ordinal) {
  return static_cast<int32_t>(d.resp_out_o[pt * kOrdinals + ordinal]);
}

void set_req_slot(Dut& d, int client, uint8_t slot) {
  uint32_t v = d.req_slot_i;
  v &= ~(0x7u << (client * 3));
  v |= (static_cast<uint32_t>(slot & 0x7) << (client * 3));
  d.req_slot_i = v;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  Dut dut;
  dut.cfg_slow_clear_i = 0;
  dut.ld_valid_i = 0;
  dut.req_valid_i = 0;
  dut.resp_ready_i = 0;
  dut.req_noprog_i = 0;
  dut.req_slot_i = 0;
  dut.rcp0_i = 0;
  dut.pc_lu_valid_i = 0;
  dut.pc_lu_resp_ready_i = 1;
  dut.pc_cm_valid_i = 0;
  dut.pc_cm_resp_ready_i = 1;
  for (int i = 0; i < kClients * kFrontPts * kInLanes; ++i) dut.req_in_i[i] = 0;
  reset(dut, 4);

  // The census's own program and the census's own stimulus, so the two runs
  // differ in the MUTATION and in nothing else.
  const uint8_t slot = 1, ob = 2;
  check(load_word(dut, kLdUop, slot, 0, instr(kOpAdd, ob, 0, 1, 0, 0)), "program uop 0", 1, 1);
  check(load_word(dut, kLdUop, slot, 1, instr(kOpEnd, 0, 0, 0, 0, 0)), "program end", 1, 1);
  check(load_word(dut, kLdOutMap, slot, 0,
                  static_cast<uint64_t>(ob) | (static_cast<uint64_t>(kSrcVectorReg & 1) << 16)),
        "program outmap ordinal 0", 1, 1);
  check(load_word(dut, kLdAssoc, slot, 0, 1ull), "program assoc", 1, 1);
  check(load_word(dut, kLdHeader, slot, 0, header_word(2, ob, 0x01, 0x01, 1, kFormCanonical)),
        "program header", 1, 1);

  for (int cl = 0; cl < kClients; ++cl) {
    set_req_slot(dut, cl, slot);
    for (int p = 0; p < kFrontPts; ++p) {
      set_req_in(dut, cl, p, 0, 11 + p);  // 11, 12, 13, 14
      set_req_in(dut, cl, p, 1, 31);      // + 31 => 42, 43, 44, 45
    }
  }

  const uint32_t all = (1u << kClients) - 1u;
  dut.req_valid_i = all;
  dut.resp_ready_i = all;

  long responses = 0, collapsed = 0, distinct = 0;
  uint8_t last_status = 0xFF;
  int32_t last_out[kFrontPts] = {0, 0, 0, 0};

  for (int t = 0; t < 4000; ++t) {
    dut.eval();
    const uint32_t ans = static_cast<uint32_t>(dut.resp_valid_o) & all;
    for (int i = 0; i < kClients; ++i) {
      if (ans & (1u << i)) {
        ++responses;
        last_status = dut.resp_status_o;
        bool all_same = true;
        for (int p = 0; p < kFrontPts; ++p) {
          last_out[p] = get_resp_out(dut, p, 0);
          if (last_out[p] != last_out[0]) all_same = false;
        }
        if (all_same) {
          ++collapsed;
        } else {
          ++distinct;
        }
      }
    }
    step(dut);
  }
  dut.req_valid_i = 0;
  dut.resp_ready_i = 0;

  std::printf("\n  === THE REPLICATING FRONT, DRIVEN WITH FOUR DISTINCT POINTS ===\n");
  std::printf("    responses                     : %ld\n", responses);
  std::printf("    last status                   : 0x%02X\n", last_status);
  std::printf("    last group's four answers     : %d %d %d %d\n", last_out[0], last_out[1],
              last_out[2], last_out[3]);
  std::printf("    COLLAPSED responses           : %ld\n", collapsed);
  std::printf("    distinct responses            : %ld\n", distinct);

  // THE MACHINE MUST BE WORKING, or the collapse below means "nothing ran"
  // rather than "the front replicated". This is the half that separates a
  // fired control from a dead one -- the depth census's own lesson, where a
  // 62x speed-up turned out to be `StNoProgram` refusals that a run-count
  // check happily passed.
  check(responses > 0, "the mutant actually ran -- a zero would make the collapse vacuous", 1,
        responses > 0 ? 1 : 0);
  check(last_status == kStOk, "the mutant's runs SUCCEEDED, so the collapse is not a refusal",
        kStOk, last_status);
  check(last_out[0] == 42, "... and point 0 -- the one it DOES evaluate -- is still right", 42,
        static_cast<uint64_t>(last_out[0]));

  // ---- THE INVERTED POLARITY --------------------------------------------
  // The census requires `collapsed == 0`. Here it must be everything.
  check(collapsed == responses,
        "THE CONTROL FIRES: every response collapsed to one repeated value",
        static_cast<uint64_t>(responses), static_cast<uint64_t>(collapsed));
  check(distinct == 0, "... and NO response carried four distinct points", 0,
        static_cast<uint64_t>(distinct));
  for (int p = 1; p < kFrontPts; ++p) {
    check(last_out[p] == 42,
          "... point p answered POINT 0's value, which is the defect this proves detectable", 42,
          static_cast<uint64_t>(last_out[p]));
  }

  std::printf("\n    SO `field_gather_front_census`'s distinct-answer check CAN FAIL,\n");
  std::printf("    and its green in that file is a measurement rather than a hope.\n");

  zhao::exit_hard(zhao::report_and_exit("field_gather_replicate_mutant"));
}
