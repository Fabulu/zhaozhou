// field_host_v2_any_not_all_mutant.cpp -- THE INVERTED-POLARITY DRIVER for
// tests/mutants/zhao_field_host_v2_any_not_all_mutant.sv.
//
// THIS PROGRAM PASSES WHEN THE MUTANT ACCEPTS AN INCOMPLETE RESULT.
//
// ---------------------------------------------------------------------------
// WHAT IT IS EVIDENCE ABOUT
// ---------------------------------------------------------------------------
// It is evidence about the INSTRUMENT, not about the design. `field_host_v2_directed`
// reports 152 green checks, and a suite that is green on its first run is the
// shape this repository distrusts most -- a checker can be green because the
// thing works or because it cannot see the thing. The question this file
// answers is narrow and it is the right one:
//
//   Does the directed test's completion verdict actually depend on the
//   ALL-versus-ANY rule, or would it pass either way?
//
// The mutant changes exactly one line -- `== req_mask_c` becomes `!= '0` -- and
// the stimulus below is the SAME stimulus the directed test drives at case 1d
// polarity B. If the suite were keyed on something incidental, this would still
// refuse and this driver would fail.
//
// ---------------------------------------------------------------------------
// THE PAIRING, WHICH IS THE WHOLE ARGUMENT
// ---------------------------------------------------------------------------
//   PRODUCTION (field_host_v2_directed, case 1d polarity B)
//       one program writing 2 ordinals, mask 0b111  ->  ST_PARTIAL, counter +1
//   MUTANT (here)
//       the identical program and mask               ->  ST_OK, counter unmoved
//
// NEITHER RUN IS EVIDENCE ALONE. The mutant by itself shows only that SOMETHING
// changed when a line changed. Production by itself shows only that it passes
// today. Together they show that the verdict turns on the completion rule and
// on nothing else -- which is what "the test discriminates" means.
//
// And note what this file does NOT do: it does not assert the bug in the
// production tree. CLAUDE.md forbids that, because such a test passes only
// while the defect exists and goes quiet the moment it is repaired. The
// assertion of CORRECT behaviour lives in the directed test; the firing lives
// here, separately.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_field_host_v2_any_not_all_mutant.h"

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
constexpr uint8_t kStPartial = 0xF3;

constexpr int kOrdinals = 7;
constexpr int kInLanes = 12;

using Dut = Vzhao_field_host_v2_any_not_all_mutant;

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

bool load_word(Dut& d, uint8_t kind, uint8_t slot, uint32_t addr, uint64_t lo64) {
  d.ld_valid_i = 1;
  d.ld_kind_i = kind;
  d.ld_slot_i = slot;
  d.ld_addr_i = addr;
  d.ld_data_i[0] = static_cast<uint32_t>(lo64 & 0xFFFFFFFFu);
  d.ld_data_i[1] = static_cast<uint32_t>(lo64 >> 32);
  d.ld_data_i[2] = 0;
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
  uint64_t w = static_cast<uint64_t>(instr_count) |
               (static_cast<uint64_t>(out_base & 0x3F) << 8);
  w |= (static_cast<uint64_t>(win_mask) & 0x7Full) << 32;
  w |= (static_cast<uint64_t>(ord_mask) & 0xFFull) << 48;
  w |= (static_cast<uint64_t>(out_count) & 0x0Full) << 56;
  w |= (static_cast<uint64_t>(form) & 0x03ull) << 60;
  return w;
}

bool load_outmap(Dut& d, uint8_t slot, uint8_t ordinal, uint8_t kind, uint16_t src) {
  return load_word(d, kLdOutMap, slot, ordinal,
                   static_cast<uint64_t>(src) | (static_cast<uint64_t>(kind & 1) << 16));
}

void set_req_slot(Dut& d, int client, uint8_t slot) {
  uint32_t v = d.req_slot_i;
  v &= ~(0x7u << (client * 3));
  v |= (static_cast<uint32_t>(slot & 0x7) << (client * 3));
  d.req_slot_i = v;
}

struct Resp {
  uint8_t present = 0;
  uint8_t status = 0xFF;
  bool got = false;
};

Resp run_once(Dut& d, int client, int budget = 60000) {
  Resp r;
  const uint32_t m = (1u << client);
  d.req_valid_i |= m;
  d.resp_ready_i |= m;
  bool accepted = false;
  for (int guard = 0; guard < budget && !r.got; ++guard) {
    d.eval();
    if (!accepted && (d.req_ready_o & m)) accepted = true;
    if (accepted && (d.resp_valid_o & m)) {
      r.present = d.resp_present_o;
      r.status = d.resp_status_o;
      r.got = true;
    }
    step(d);
    if (accepted) d.req_valid_i &= ~m;
  }
  d.req_valid_i &= ~m;
  d.resp_ready_i &= ~m;
  return r;
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
  for (int i = 0; i < 2 * kInLanes; ++i) dut.req_in_i[i] = 0;
  reset(dut, 4);

  std::printf("ANY-NOT-ALL MUTANT: the SAME stimulus as field_host_v2_directed\n");
  std::printf("ANY-NOT-ALL MUTANT: case 1d polarity B, against a host whose only\n");
  std::printf("ANY-NOT-ALL MUTANT: difference is `== req_mask_c` -> `!= 0`.\n");

  const uint8_t slot = 6, ob = 3;
  check(load_word(dut, kLdUop, slot, 0, instr(kOpAdd, ob, 0, 1, 0, 0)), "uop 0 loads", 1, 1);
  check(load_word(dut, kLdUop, slot, 1, instr(kOpAdd, ob + 1, 0, 1, 0, 0)), "uop 1 loads", 1, 1);
  check(load_word(dut, kLdUop, slot, 2, instr(kOpEnd, 0, 0, 0, 0, 0)), "end loads", 1, 1);
  check(load_outmap(dut, slot, 0, kSrcVectorReg, ob), "ordinal 0 maps", 1, 1);
  check(load_outmap(dut, slot, 1, kSrcVectorReg, ob + 1), "ordinal 1 maps", 1, 1);
  check(load_outmap(dut, slot, 2, kSrcVectorReg, ob + 2), "ordinal 2 maps", 1, 1);
  check(load_word(dut, kLdAssoc, slot, 0, 1), "assoc loads", 1, 1);

  set_req_slot(dut, 0, slot);
  dut.req_in_i[0] = 6;
  dut.req_in_i[1] = 6;

  // ---- POLARITY A: a TRUE declaration. Both hosts must accept this. -------
  // Without this the mutant would be indistinguishable from a host that
  // accepts everything, and "it said OK" would be worth nothing.
  check(load_word(dut, kLdHeader, slot, 0, header_word(3, ob, 0x03, 0x03, 2, kFormCanonical)),
        "header mask 0b011 loads", 1, 1);
  Resp a = run_once(dut, 0);
  check(a.got, "the mutant answers a true declaration", 1, a.got ? 1 : 0);
  check(a.status == kStOk, "MUTANT CONTROL: a truthful mask is still accepted", 0x00, a.status);

  // ---- POLARITY B: the declaration claims a THIRD ordinal nobody wrote ----
  // Production refuses this with ST_PARTIAL (asserted in field_host_v2_directed).
  // The mutant must ACCEPT it -- that is the R101 defect executing.
  const uint32_t before = dut.out_incomplete_o;
  check(load_word(dut, kLdHeader, slot, 0, header_word(3, ob, 0x03, 0x07, 3, kFormCanonical)),
        "header mask 0b111 loads", 1, 1);
  Resp b = run_once(dut, 0);
  check(b.got, "the mutant answers", 1, b.got ? 1 : 0);

  // THE INVERTED ASSERTION. Two ordinals of three present, and the mutant calls
  // it success: a caller would read the third word as the zero cleared at
  // grant, indistinguishable from a field that is genuinely zero there.
  check(b.present == 0x03, "only two of three ordinals really landed", 0x03, b.present);
  check(b.status == kStOk,
        "MUTANT FIRES: ANY-not-ALL accepts an INCOMPLETE result (production refuses 0xF3)",
        0x00, b.status);
  check(dut.out_incomplete_o == before,
        "MUTANT FIRES: out_incomplete_o stayed silent on a partial result", before,
        dut.out_incomplete_o);

  if (b.status == kStOk && b.present == 0x03) {
    std::printf("ANY-NOT-ALL MUTANT: FIRED. The directed test's case 1d therefore\n");
    std::printf("ANY-NOT-ALL MUTANT: discriminates on the completion rule itself:\n");
    std::printf("ANY-NOT-ALL MUTANT: production answers 0xF3 to this exact stimulus,\n");
    std::printf("ANY-NOT-ALL MUTANT: this copy answers 0x%02X, and one line separates them.\n",
                b.status);
  } else {
    std::printf("ANY-NOT-ALL MUTANT: DID NOT FIRE -- status 0x%02X, present 0x%02X.\n", b.status,
                b.present);
    std::printf("ANY-NOT-ALL MUTANT: a mutant that does not fire is a green control\n");
    std::printf("ANY-NOT-ALL MUTANT: attached to nothing (ruling R123). Either the copy\n");
    std::printf("ANY-NOT-ALL MUTANT: has gone stale against production, or the stimulus\n");
    std::printf("ANY-NOT-ALL MUTANT: no longer reaches the mutated line.\n");
  }

  return zhao::report_and_exit("field_host_v2_any_not_all_mutant");
}
