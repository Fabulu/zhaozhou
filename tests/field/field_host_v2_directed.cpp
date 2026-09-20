// field_host_v2_directed.cpp -- the association-aware FIELD host.
//
// ENFORCES: fpga/rtl/field/zhao_field_host_v2.sv
// Contract: design/contracts/FIELD.SEQ.CORE.md
// Schema:   fpga/rtl/field/generated/zhao_field_host_image_pkg.sv
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE IS FOR, AND WHAT IT DELIBERATELY DOES NOT TEST
// ---------------------------------------------------------------------------
// The ARITHMETIC is not re-tested here. `zhao_field_v3_engine` is the fabric
// and tests/differential/field_v3_full_directed.cpp is its differential against
// `zfield::interpret`. Every case below is about the HOST's output contract --
// which canonical output ordinals a point owes, whether it produced them, and
// whether a caller can tell a value from a hole.
//
// THE CASE THAT DISCRIMINATES IS NAMED IN EVERY BLOCK. A test count is not
// coverage: what makes a case evidence is that it distinguishes the repair from
// a plausible wrong implementation, and each block says which wrong
// implementation it would catch.
//
// ---------------------------------------------------------------------------
// FT018 AND FT019 ARE DRIVEN INDEPENDENTLY, WHICH IS THE WHOLE POINT
// ---------------------------------------------------------------------------
// Directive FT019: "Missing-value and zero-value cases must be distinguished by
// independently driven validity." The two cases below therefore differ in WHAT
// THE PROGRAM DOES, not in what value it computes:
//
//   FT018 omits a WRITE     -> the ordinal is never seen  -> ST_PARTIAL
//   FT019 writes the VALUE 0 -> the ordinal is seen        -> OK
//
// A host that inferred presence from the data would pass one and fail the
// other. A host that inferred it from "did anything land in the window" would
// pass both and be wrong, which is the defect R101 found.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_host_v2.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

// spec/form/field-ir.md canonical opcodes.
constexpr uint8_t kOpEnd = 0x00;
constexpr uint8_t kOpAdd = 0x03;

// The host's load kinds. The oracle's four keep their encodings; four are new.
constexpr uint8_t kLdUop = 0;
constexpr uint8_t kLdHeader = 2;
constexpr uint8_t kLdOutMap = 4;
constexpr uint8_t kLdAssoc = 5;
constexpr uint8_t kLdInitProof = 6;
constexpr uint8_t kLdPrepared = 7;

// OUTPUT_MAP.source_kind, from the generated schema.
constexpr uint8_t kSrcVectorReg = 0;
constexpr uint8_t kSrcPreparedScalar = 1;

// PROGRAM_META.execution_form, from the generated schema.
constexpr uint8_t kFormCanonical = 0;
constexpr uint8_t kFormUniformOnly = 2;

// Statuses.
constexpr uint8_t kStOk = 0x00;
constexpr uint8_t kStNoProgram = 0xF0;
constexpr uint8_t kStNoResult = 0xF1;
constexpr uint8_t kStPartial = 0xF3;
constexpr uint8_t kStBadPrep = 0xF4;
constexpr uint8_t kStBadImage = 0xF5;

// The composed shape this bench takes (the module's own defaults).
constexpr int kOrdinals = 7;
constexpr int kInLanes = 12;

using Dut = Vzhao_field_host_v2;

void step(Dut& d) { zhao::tick(d); }

void reset(Dut& d, int cycles) {
  d.rst_n = 0;
  for (int i = 0; i < cycles; ++i) step(d);
  d.rst_n = 1;
  step(d);
}

//   [7:0] op  [13:8] dst  [19:14] a  [25:20] b  [31:26] c  [63:32] imm
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

// THE v2 HEADER WORD. The WINDOW mask keeps R101's home at [32 +: OUT_LANES];
// the ORDINAL mask sits above it at [48 +: 8]. They are different fields of
// different widths because they are different quantities, and this helper is
// the only place in the test that knows the layout.
//   [13:8] out_base   [22:16] tbl_n0   [31:24] tbl_n1
//   [32 +: 7] WINDOW mask   [48 +: 8] ORDINAL mask
//   [56 +: 4] output_count  [60 +: 2] execution_form
uint64_t header_lo(uint8_t count, uint8_t out_base) {
  return static_cast<uint64_t>(count) | (static_cast<uint64_t>(out_base & 0x3F) << 8);
}
uint32_t header_hi(uint8_t win_mask, uint8_t ord_mask, uint8_t out_count, uint8_t form) {
  // bits 64..95 of the 96-bit word; the fields above live in the low 64.
  (void)win_mask;
  (void)ord_mask;
  (void)out_count;
  (void)form;
  return 0;
}

// The header fields at 32..63 live in the HIGH half of the low 64-bit word, so
// they are packed here rather than in `hi32`.
uint64_t header_word(uint8_t instr_count, uint8_t out_base, uint8_t win_mask, uint8_t ord_mask,
                     uint8_t out_count, uint8_t form) {
  uint64_t w = header_lo(instr_count, out_base);
  w |= (static_cast<uint64_t>(win_mask) & 0x7Full) << 32;
  w |= (static_cast<uint64_t>(ord_mask) & 0xFFull) << 48;
  w |= (static_cast<uint64_t>(out_count) & 0x0Full) << 56;
  w |= (static_cast<uint64_t>(form) & 0x03ull) << 60;
  return w;
}

// One OUTPUT_MAP row. `addr` is the canonical output ORDINAL.
//   [15:0] source_index   [16] source_kind
bool load_outmap(Dut& d, uint8_t slot, uint8_t ordinal, uint8_t kind, uint16_t src) {
  uint64_t w = static_cast<uint64_t>(src) | (static_cast<uint64_t>(kind & 1) << 16);
  return load_word(d, kLdOutMap, slot, ordinal, w);
}

// ASSOCIATION_META: [7:0] generation.
bool load_assoc(Dut& d, uint8_t slot, uint8_t gen) {
  return load_word(d, kLdAssoc, slot, 0, static_cast<uint64_t>(gen));
}

// INIT_PROOF: bit 0 = the proof was accepted, so the per-point clear may be
// skipped. The PROOF itself is the image's and is walked by the C++ validator;
// this bit is the hardware interlock and nothing else.
bool load_initproof(Dut& d, uint8_t slot, bool ok) {
  return load_word(d, kLdInitProof, slot, 0, ok ? 1ull : 0ull);
}

// A prepared scalar. `addr` is the prepared slot.
//   [31:0] value   [32] VALID   [47:40] generation
//
// THE VALID BIT IS DRIVEN INDEPENDENTLY OF THE VALUE, which is what lets FT019
// write a genuine zero and FT018 withhold a value without the two looking alike.
bool load_prepared(Dut& d, uint8_t slot, uint8_t index, int32_t value, bool valid, uint8_t gen) {
  uint64_t w = static_cast<uint32_t>(value);
  if (valid) w |= (1ull << 32);
  w |= (static_cast<uint64_t>(gen) << 40);
  return load_word(d, kLdPrepared, slot, index, w);
}

void set_req_in(Dut& d, int client, int lane, int32_t value) {
  d.req_in_i[client * kInLanes + lane] = static_cast<uint32_t>(value);
}

void set_req_slot(Dut& d, int client, uint8_t slot) {
  uint32_t v = d.req_slot_i;
  v &= ~(0x7u << (client * 3));
  v |= (static_cast<uint32_t>(slot & 0x7) << (client * 3));
  d.req_slot_i = v;
}

// The whole response, captured at the instant `resp_valid_o` is seen. Reading
// these ports after `run_once` returns would read them after the entry has been
// released, which is a real hazard now that delivery is decoupled from the run.
struct Resp {
  int32_t out[kOrdinals] = {0, 0, 0, 0, 0, 0, 0};
  uint8_t present = 0;
  uint8_t window = 0;
  uint8_t count = 0;
  uint8_t status = 0xFF;
  uint8_t num = 0;
  bool got = false;
};

// `tick` samples inputs at the rising edge and leaves the model settled at
// clk=0, so a handshake is observed by EVALUATING first and stepping after.
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
      for (int l = 0; l < kOrdinals; ++l) r.out[l] = static_cast<int32_t>(d.resp_out_o[l]);
      r.present = d.resp_present_o;
      r.window = d.resp_window_o;
      r.count = d.resp_count_o;
      r.status = d.resp_status_o;
      r.num = d.num_status_o;
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

  // =======================================================================
  // FT017. A ONE-OUTPUT PROGRAM ON A SEVEN-WORD BUS SUCCEEDS, AND THE
  // UNDECLARED WINDOW LANES ARE REPORTED AS PADDING, NOT AS MISSING RESULTS.
  // =======================================================================
  // THE CASE THAT DISCRIMINATES: a host that required the whole seven-lane
  // window to be written would refuse this, and a host that reported the six
  // untouched lanes as absent results would make a correct program look broken.
  // Padding is a property of the DECLARATION, not of what happened to land.
  {
    const uint8_t slot = 1, ob = 2;
    check(load_word(dut, kLdUop, slot, 0, instr(kOpAdd, ob, 0, 1, 0, 0)), "FT017 uop 0", 1, 1);
    check(load_word(dut, kLdUop, slot, 1, instr(kOpEnd, 0, 0, 0, 0, 0)), "FT017 end", 1, 1);
    check(load_outmap(dut, slot, 0, kSrcVectorReg, ob), "FT017 outmap ordinal 0", 1, 1);
    check(load_assoc(dut, slot, 1), "FT017 assoc", 1, 1);
    // window mask 0b0000001 (lane 0 of the window is claimed), ordinal mask
    // 0b0000001, count 1.
    check(load_word(dut, kLdHeader, slot, 0, header_word(2, ob, 0x01, 0x01, 1, kFormCanonical)),
          "FT017 header", 1, 1);

    set_req_slot(dut, 0, slot);
    set_req_in(dut, 0, 0, 11);
    set_req_in(dut, 0, 1, 31);
    Resp r = run_once(dut, 0);
    check(r.got, "FT017 completes", 1, r.got ? 1 : 0);
    check(r.status == kStOk, "FT017 one declared output succeeds on a seven-word bus", 0x00,
          r.status);
    check(r.out[0] == 42, "FT017 ordinal 0 carries R0+R1", 42, static_cast<uint64_t>(r.out[0]));
    check(r.present == 0x01, "FT017 present mask names exactly ordinal 0", 0x01, r.present);
    check(r.count == 1, "FT017 the declared arity comes back with the result", 1, r.count);
    // Six undeclared window lanes + the written one = all seven accounted for.
    check(r.window == 0x7F, "FT017 undeclared window lanes report as PADDING, not missing", 0x7F,
          r.window);
    check(dut.runs_o == 1, "FT017 runs_o fired", 1, dut.runs_o);
    check(dut.no_result_o == 0, "FT017 no_result_o silent", 0, dut.no_result_o);
    check(dut.out_incomplete_o == 0, "FT017 out_incomplete_o silent", 0, dut.out_incomplete_o);
  }

  // =======================================================================
  // FT020. SPARSE OUTPUT REGISTERS RETURN IN DECLARED OUTPUT ORDER.
  // =======================================================================
  // THE CASE THAT DISCRIMINATES, and it is the whole reason this host exists.
  // The three ordinals are mapped to registers in an order that is NOT the
  // ordinal order and NOT contiguous:
  //
  //   ordinal 0 <- R7      ordinal 1 <- R4      ordinal 2 <- R9
  //
  // with out_base = 4. A host indexing by window position would return
  // R4, R7, R9 -- physical order -- and every value would be in the wrong slot
  // while every value was individually correct. That is the failure a
  // result-checking test cannot see unless the registers are deliberately
  // shuffled, so they are.
  {
    const uint8_t slot = 2, ob = 4;
    check(load_word(dut, kLdUop, slot, 0, instr(kOpAdd, 7, 0, 1, 0, 0)), "FT020 R7 = R0+R1", 1, 1);
    check(load_word(dut, kLdUop, slot, 1, instr(kOpAdd, 4, 0, 2, 0, 0)), "FT020 R4 = R0+R2", 1, 1);
    check(load_word(dut, kLdUop, slot, 2, instr(kOpAdd, 9, 1, 2, 0, 0)), "FT020 R9 = R1+R2", 1, 1);
    check(load_word(dut, kLdUop, slot, 3, instr(kOpEnd, 0, 0, 0, 0, 0)), "FT020 end", 1, 1);
    check(load_outmap(dut, slot, 0, kSrcVectorReg, 7), "FT020 ordinal 0 <- R7", 1, 1);
    check(load_outmap(dut, slot, 1, kSrcVectorReg, 4), "FT020 ordinal 1 <- R4", 1, 1);
    check(load_outmap(dut, slot, 2, kSrcVectorReg, 9), "FT020 ordinal 2 <- R9", 1, 1);
    check(load_assoc(dut, slot, 1), "FT020 assoc", 1, 1);
    // Window lanes touched relative to out_base=4: R4->0, R7->3, R9->5.
    check(load_word(dut, kLdHeader, slot, 0, header_word(4, ob, 0x29, 0x07, 3, kFormCanonical)),
          "FT020 header", 1, 1);

    set_req_slot(dut, 0, slot);
    set_req_in(dut, 0, 0, 100);
    set_req_in(dut, 0, 1, 20);
    set_req_in(dut, 0, 2, 3);
    Resp r = run_once(dut, 0);
    check(r.got, "FT020 completes", 1, r.got ? 1 : 0);
    check(r.status == kStOk, "FT020 sparse non-contiguous outputs succeed", 0x00, r.status);
    // The discriminating assertions: ordinal order, not register order.
    check(r.out[0] == 120, "FT020 ordinal 0 is R7's value (R0+R1), not R4's", 120,
          static_cast<uint64_t>(r.out[0]));
    check(r.out[1] == 103, "FT020 ordinal 1 is R4's value (R0+R2), not R7's", 103,
          static_cast<uint64_t>(r.out[1]));
    check(r.out[2] == 23, "FT020 ordinal 2 is R9's value (R1+R2)", 23,
          static_cast<uint64_t>(r.out[2]));
    check(r.present == 0x07, "FT020 three ordinals present", 0x07, r.present);
  }

  // =======================================================================
  // FT018. A DECLARED ORDINAL THAT WAS NEVER WRITTEN IS A REFUSAL.
  // =======================================================================
  // Three ordinals declared, TWO written. The third's register is never a
  // destination, so the ordinal is never seen and the word would read as the
  // zero cleared at grant -- W10's "Do not make an absent output look like a
  // zero result". The value is withheld by OMITTING AN INSTRUCTION, which is
  // what makes this independent of FT019 below.
  {
    const uint8_t slot = 3, ob = 4;
    const uint32_t before = dut.out_incomplete_o;
    check(load_word(dut, kLdUop, slot, 0, instr(kOpAdd, 4, 0, 1, 0, 0)), "FT018 R4", 1, 1);
    check(load_word(dut, kLdUop, slot, 1, instr(kOpAdd, 5, 0, 1, 0, 0)), "FT018 R5", 1, 1);
    check(load_word(dut, kLdUop, slot, 2, instr(kOpEnd, 0, 0, 0, 0, 0)), "FT018 end", 1, 1);
    check(load_outmap(dut, slot, 0, kSrcVectorReg, 4), "FT018 ordinal 0 <- R4", 1, 1);
    check(load_outmap(dut, slot, 1, kSrcVectorReg, 5), "FT018 ordinal 1 <- R5", 1, 1);
    // Ordinal 2 is DECLARED and its register is never written.
    check(load_outmap(dut, slot, 2, kSrcVectorReg, 6), "FT018 ordinal 2 <- R6 (never written)", 1,
          1);
    check(load_assoc(dut, slot, 1), "FT018 assoc", 1, 1);
    check(load_word(dut, kLdHeader, slot, 0, header_word(3, ob, 0x07, 0x07, 3, kFormCanonical)),
          "FT018 header", 1, 1);

    set_req_slot(dut, 0, slot);
    set_req_in(dut, 0, 0, 7);
    set_req_in(dut, 0, 1, 5);
    Resp r = run_once(dut, 0);
    check(r.got, "FT018 completes", 1, r.got ? 1 : 0);
    check(r.status == kStPartial, "FT018 an omitted declared output REFUSES", 0xF3, r.status);
    check(r.present == 0x03, "FT018 present names the two that landed, not the third", 0x03,
          r.present);
    check(dut.out_incomplete_o == before + 1, "FT018 out_incomplete_o fired", before + 1,
          dut.out_incomplete_o);
  }

  // =======================================================================
  // FT019. SIX ACTUAL ZERO-VALUED OUTPUTS SUCCEED.
  // =======================================================================
  // INDEPENDENTLY DRIVEN FROM FT018: every declared ordinal IS written, and the
  // value written is genuinely zero (R10 + R11, both zero). A host that
  // inferred presence from the data would report these as missing and refuse.
  // The pair FT018/FT019 is the discriminator: same status question, opposite
  // answers, and the only thing that differs is whether the write happened.
  {
    const uint8_t slot = 4, ob = 2;
    const uint32_t part_before = dut.out_incomplete_o;
    for (int i = 0; i < 6; ++i) {
      check(load_word(dut, kLdUop, slot, i, instr(kOpAdd, ob + i, 10, 11, 0, 0)),
            "FT019 zero-valued write", 1, 1);
    }
    check(load_word(dut, kLdUop, slot, 6, instr(kOpEnd, 0, 0, 0, 0, 0)), "FT019 end", 1, 1);
    for (int j = 0; j < 6; ++j) {
      check(load_outmap(dut, slot, static_cast<uint8_t>(j), kSrcVectorReg,
                        static_cast<uint16_t>(ob + j)),
            "FT019 outmap", 1, 1);
    }
    check(load_assoc(dut, slot, 1), "FT019 assoc", 1, 1);
    check(load_word(dut, kLdHeader, slot, 0, header_word(7, ob, 0x3F, 0x3F, 6, kFormCanonical)),
          "FT019 header", 1, 1);

    set_req_slot(dut, 0, slot);
    // R10 and R11 are never written by this program and are cleared at grant,
    // so every output is a genuine computed zero.
    Resp r = run_once(dut, 0);
    check(r.got, "FT019 completes", 1, r.got ? 1 : 0);
    check(r.status == kStOk, "FT019 six genuinely ZERO outputs SUCCEED", 0x00, r.status);
    check(r.present == 0x3F, "FT019 all six ordinals present despite being zero", 0x3F, r.present);
    for (int j = 0; j < 6; ++j) {
      check(r.out[j] == 0, "FT019 the value really is zero", 0, static_cast<uint64_t>(r.out[j]));
    }
    check(dut.out_incomplete_o == part_before, "FT019 zero values do NOT count as incomplete",
          part_before, dut.out_incomplete_o);
  }

  // =======================================================================
  // FT021 + FT023. A UNIFORM-ONLY PROGRAM RETURNS WITHOUT A VECTOR WRITE,
  // AND TWO ORDINALS MAY ALIAS ONE PREPARED SCALAR.
  // =======================================================================
  // This is FH06 in a test. The program has NO instructions at all -- not an
  // empty vector body that ends immediately, but an image whose declared form
  // is UNIFORM_ONLY, so the host must not start a context for it. A host using
  // the oracle's rule (`out_seen == 0` -> ST_NO_RESULT) can only refuse this,
  // and a host that started the fabric would park a context waiting for an END
  // that no instruction will ever issue.
  //
  // L1 measured that this is not hypothetical: all three shipped Earth programs
  // carry uniform outputs -- crater_ring 2 of 4, impact_wave 1, wave_pool 1.
  {
    const uint8_t slot = 5;
    const uint32_t uni_before = dut.uniform_runs_o;
    check(load_prepared(dut, slot, 3, 1234, true, 9), "FT021 prepared slot 3 = 1234", 1, 1);
    check(load_prepared(dut, slot, 4, -77, true, 9), "FT021 prepared slot 4 = -77", 1, 1);
    // ORDINALS 0 AND 2 BOTH NAME PREPARED SLOT 3 -- legal aliasing (FT023).
    check(load_outmap(dut, slot, 0, kSrcPreparedScalar, 3), "FT021 ordinal 0 <- prep 3", 1, 1);
    check(load_outmap(dut, slot, 1, kSrcPreparedScalar, 4), "FT021 ordinal 1 <- prep 4", 1, 1);
    check(load_outmap(dut, slot, 2, kSrcPreparedScalar, 3), "FT023 ordinal 2 <- prep 3 (alias)", 1,
          1);
    check(load_assoc(dut, slot, 9), "FT021 assoc generation 9", 1, 1);
    check(load_word(dut, kLdHeader, slot, 0, header_word(0, 0, 0x00, 0x07, 3, kFormUniformOnly)),
          "FT021 header declares UNIFORM_ONLY", 1, 1);

    set_req_slot(dut, 0, slot);
    Resp r = run_once(dut, 0);
    check(r.got, "FT021 a uniform-only program RETURNS", 1, r.got ? 1 : 0);
    check(r.status == kStOk, "FT021 uniform-only succeeds without any vector write", 0x00,
          r.status);
    check(r.present == 0x07, "FT021 all three declared ordinals present", 0x07, r.present);
    check(r.out[0] == 1234, "FT021 ordinal 0 carries the prepared scalar", 1234,
          static_cast<uint64_t>(r.out[0]));
    check(r.out[1] == -77, "FT021 ordinal 1 carries its own prepared scalar", -77,
          static_cast<uint64_t>(static_cast<int64_t>(r.out[1])));
    check(r.out[2] == 1234, "FT023 the aliasing ordinal carries the SAME prepared scalar", 1234,
          static_cast<uint64_t>(r.out[2]));
    check(dut.uniform_runs_o == uni_before + 1, "FT021 uniform_runs_o fired", uni_before + 1,
          dut.uniform_runs_o);
  }

  // =======================================================================
  // FT022 / FT038. REMOVE A PREPARED SCALAR'S VALID BIT AND THE POINT FAILS.
  // =======================================================================
  // THE POSITIVE CONTROL FOR FT021, and the reason validity is a separate bit.
  // The VALUE is left exactly as it was; only the VALID bit is cleared. A host
  // that treated "the RAM holds a number" as "the preparation happened" would
  // pass this and be wrong -- directive FT038: "zero-filled RAM is not a
  // substitute".
  {
    const uint8_t slot = 5;
    const uint32_t bad_before = dut.prep_bad_o;
    // Same value, same generation, VALID CLEARED. Nothing else moves.
    check(load_prepared(dut, slot, 4, -77, false, 9), "FT022 clear ONLY the valid bit", 1, 1);
    check(load_word(dut, kLdHeader, slot, 0, header_word(0, 0, 0x00, 0x07, 3, kFormUniformOnly)),
          "FT022 header", 1, 1);

    set_req_slot(dut, 0, slot);
    Resp r = run_once(dut, 0);
    check(r.got, "FT022 completes", 1, r.got ? 1 : 0);
    check(r.status == kStBadPrep, "FT022 an unprepared declared scalar REFUSES", 0xF4, r.status);
    check(dut.prep_bad_o == bad_before + 1, "FT022 prep_bad_o fired", bad_before + 1,
          dut.prep_bad_o);
  }

  // =======================================================================
  // FT036. A MATCHING PROGRAM IS NOT A MATCHING PARAMETER SET.
  // =======================================================================
  // Directive 6.4: "do not reuse a previously loaded uniform register unless
  // its association-generation tag matches. A matching program hash is not a
  // matching parameter set." The prepared values and their valid bits are
  // restored exactly; only the ASSOCIATION GENERATION moves. The point must
  // refuse rather than silently returning the previous association's numbers,
  // which is the wrong answer nobody could see.
  {
    const uint8_t slot = 5;
    const uint32_t bad_before = dut.prep_bad_o;
    check(load_prepared(dut, slot, 3, 1234, true, 9), "FT036 restore prep 3", 1, 1);
    check(load_prepared(dut, slot, 4, -77, true, 9), "FT036 restore prep 4 VALID", 1, 1);
    // A new association generation over the SAME program and the SAME
    // preparation storage.
    check(load_assoc(dut, slot, 10), "FT036 association generation 9 -> 10", 1, 1);
    check(load_word(dut, kLdHeader, slot, 0, header_word(0, 0, 0x00, 0x07, 3, kFormUniformOnly)),
          "FT036 header", 1, 1);

    set_req_slot(dut, 0, slot);
    Resp r = run_once(dut, 0);
    check(r.got, "FT036 completes", 1, r.got ? 1 : 0);
    check(r.status == kStBadPrep, "FT036 a stale preparation generation REFUSES", 0xF4, r.status);
    check(dut.prep_bad_o == bad_before + 1, "FT036 prep_bad_o fired on the generation",
          bad_before + 1, dut.prep_bad_o);
  }

  // =======================================================================
  // CASE 1d, CARRIED FORWARD FROM gz/fieldp4. THE PERMANENT BOTH-POLARITY
  // CONTROL, TRANSLATED FROM WINDOW SPACE INTO ORDINAL SPACE.
  // =======================================================================
  // ONE program, run TWICE, with ONLY THE MASK MOVED. It writes two ordinals.
  // Mask 0b011 -> the declaration is true  -> OK.
  // Mask 0b111 -> the declaration claims a third -> REFUSED.
  //
  // This is the shape that makes the guard evidence rather than a green: the
  // only thing that differs between the accept and the refusal is the field the
  // detector reads. It asserts the CORRECT behaviour in both directions, so it
  // keeps passing after the defect is gone -- which a test that asserted the
  // bug would not.
  {
    const uint8_t slot = 6, ob = 3;
    check(load_word(dut, kLdUop, slot, 0, instr(kOpAdd, ob, 0, 1, 0, 0)), "1d uop 0", 1, 1);
    check(load_word(dut, kLdUop, slot, 1, instr(kOpAdd, ob + 1, 0, 1, 0, 0)), "1d uop 1", 1, 1);
    check(load_word(dut, kLdUop, slot, 2, instr(kOpEnd, 0, 0, 0, 0, 0)), "1d end", 1, 1);
    check(load_outmap(dut, slot, 0, kSrcVectorReg, ob), "1d ordinal 0", 1, 1);
    check(load_outmap(dut, slot, 1, kSrcVectorReg, ob + 1), "1d ordinal 1", 1, 1);
    check(load_outmap(dut, slot, 2, kSrcVectorReg, ob + 2), "1d ordinal 2", 1, 1);
    check(load_assoc(dut, slot, 1), "1d assoc", 1, 1);

    set_req_slot(dut, 0, slot);
    set_req_in(dut, 0, 0, 6);
    set_req_in(dut, 0, 1, 6);

    // --- polarity A: the declaration is TRUE ---
    check(load_word(dut, kLdHeader, slot, 0, header_word(3, ob, 0x03, 0x03, 2, kFormCanonical)),
          "1d header mask 0b011", 1, 1);
    Resp a = run_once(dut, 0);
    check(a.got, "1d(A) completes", 1, a.got ? 1 : 0);
    check(a.status == kStOk, "1d(A) mask 0b011 over two written ordinals is OK", 0x00, a.status);
    check(a.out[0] == 12, "1d(A) ordinal 0 value", 12, static_cast<uint64_t>(a.out[0]));

    // --- polarity B: ONLY THE MASK MOVES ---
    const uint32_t before = dut.out_incomplete_o;
    check(load_word(dut, kLdHeader, slot, 0, header_word(3, ob, 0x03, 0x07, 3, kFormCanonical)),
          "1d header mask 0b111", 1, 1);
    Resp b = run_once(dut, 0);
    check(b.got, "1d(B) completes", 1, b.got ? 1 : 0);
    check(b.status == kStPartial, "1d(B) the SAME program with mask 0b111 is REFUSED", 0xF3,
          b.status);
    check(dut.out_incomplete_o == before + 1, "1d(B) out_incomplete_o fired", before + 1,
          dut.out_incomplete_o);
  }

  // =======================================================================
  // FT030 / R111. A ZERO ORDINAL MASK THAT DECLARES OUTPUTS IS REFUSED.
  // =======================================================================
  // R111's standing hazard: "a plan writer who omits the mask silently restores
  // the defect and passes every gate". Zero never means "accept whatever
  // happened" here -- the descriptor contradicts itself and the slot does not
  // become runnable, so the point answers ST_NO_PROGRAM rather than running and
  // succeeding on whatever landed.
  {
    const uint8_t slot = 7, ob = 2;
    const uint32_t zm_before = dut.zero_mask_o;
    check(load_word(dut, kLdUop, slot, 0, instr(kOpAdd, ob, 0, 1, 0, 0)), "FT030 uop", 1, 1);
    check(load_word(dut, kLdUop, slot, 1, instr(kOpEnd, 0, 0, 0, 0, 0)), "FT030 end", 1, 1);
    check(load_outmap(dut, slot, 0, kSrcVectorReg, ob), "FT030 outmap", 1, 1);
    check(load_assoc(dut, slot, 1), "FT030 assoc", 1, 1);
    // count = 1 but ORDINAL MASK = 0. The descriptor contradicts itself.
    check(load_word(dut, kLdHeader, slot, 0, header_word(2, ob, 0x01, 0x00, 1, kFormCanonical)),
          "FT030 header with a ZERO ordinal mask", 1, 1);
    check(dut.zero_mask_o == zm_before + 1, "FT030 zero_mask_o fired at LOAD", zm_before + 1,
          dut.zero_mask_o);

    set_req_slot(dut, 0, slot);
    Resp r = run_once(dut, 0);
    check(r.got, "FT030 completes", 1, r.got ? 1 : 0);
    check(r.status == kStNoProgram, "FT030 a zero-mask descriptor never becomes runnable", 0xF0,
          r.status);
  }

  // =======================================================================
  // BAD IMAGE: AN ORDINAL THE CAPTURE WINDOW CANNOT OBSERVE.
  // =======================================================================
  // Section 2 case 2 of the repair plan. A VECTOR_REG source outside
  // [out_base, out_base + OUT_LANES) can never be seen, so the ordinal could
  // never be satisfied and the point would refuse for the WRONG REASON -- which
  // sends the next person to the completion logic instead of to the image.
  // Refused at LOAD, by name, and the counter is seen to move.
  //
  // THE CHECK LIVES AT THE HEADER, NOT AT THE MAP ROW, and the ordering is the
  // reason. `out_base` arrives in the header and the header is written LAST --
  // it is the write that makes a slot runnable. A row validated when it loads
  // would be measured against the PREVIOUS program's base. The first version of
  // this host did exactly that and refused two of FT020's three perfectly good
  // ordinals; the header is the first moment the descriptor is complete.
  {
    const uint8_t slot = 0, ob = 2;
    const uint32_t bi_before = dut.bad_image_o;
    check(load_word(dut, kLdUop, slot, 0, instr(kOpAdd, ob, 0, 1, 0, 0)), "BADIMAGE uop", 1, 1);
    check(load_word(dut, kLdUop, slot, 1, instr(kOpEnd, 0, 0, 0, 0, 0)), "BADIMAGE end", 1, 1);
    check(load_outmap(dut, slot, 0, kSrcVectorReg, ob), "BADIMAGE ordinal 0 <- R2 (inside)", 1, 1);
    // out_base is 2 and OUT_LANES is 7, so the window is R2..R8. R20 is outside
    // it, so a write to R20 could never be observed and ordinal 1 could never
    // be satisfied.
    check(load_outmap(dut, slot, 1, kSrcVectorReg, 20), "BADIMAGE ordinal 1 <- R20 (outside)", 1,
          1);
    check(load_assoc(dut, slot, 1), "BADIMAGE assoc", 1, 1);
    check(load_word(dut, kLdHeader, slot, 0, header_word(2, ob, 0x01, 0x03, 2, kFormCanonical)),
          "BADIMAGE header completes the descriptor", 1, 1);
    check(dut.bad_image_o == bi_before + 1, "BADIMAGE bad_image_o fired at the HEADER",
          bi_before + 1, dut.bad_image_o);

    // And the slot did NOT become runnable, so the point refuses by name rather
    // than running and then failing for a reason that points at the wrong file.
    set_req_slot(dut, 0, slot);
    Resp r = run_once(dut, 0);
    check(r.got, "BADIMAGE completes", 1, r.got ? 1 : 0);
    check(r.status == kStNoProgram, "BADIMAGE an unobservable ordinal never becomes runnable",
          0xF0, r.status);
  }

  // =======================================================================
  // THE rcp0 PORT CAN SEE THE THING. (Rule: every new observation port owes a
  // demonstration that it can SEE its subject, not merely that it compiles.)
  // =======================================================================
  // `num_status_o` is four bits where the oracle's `sat_o` is three, and the
  // fourth is rcp0 -- the cause `zhao_field_v3_svcpath.sv:437` carries as
  // `nm_rcp0_unconsumed` precisely because the destination did not exist.
  //
  // The producer chain ABOVE this file is incomplete (rcp0 has no port on
  // svcpath, dispatch, core or engine), so what is demonstrated here is exactly
  // what this packet owns: that the bit, once driven, reaches the boundary in
  // its OWN family and does not get folded into a saturation bit. Driving the
  // input is not a substitute for the chain and is not reported as one.
  {
    const uint8_t slot = 2;
    set_req_slot(dut, 0, slot);
    set_req_in(dut, 0, 0, 1);
    set_req_in(dut, 0, 1, 1);
    set_req_in(dut, 0, 2, 1);

    dut.rcp0_i = 0;
    Resp quiet = run_once(dut, 0);
    check(quiet.got, "rcp0 negative control completes", 1, quiet.got ? 1 : 0);
    check((quiet.num & 0x8) == 0, "rcp0 bit is CLEAR when the cause is absent", 0,
          quiet.num & 0x8);

    dut.rcp0_i = 1;
    Resp fired = run_once(dut, 0);
    dut.rcp0_i = 0;
    check(fired.got, "rcp0 positive control completes", 1, fired.got ? 1 : 0);
    check((fired.num & 0x8) != 0, "rcp0 REACHES the boundary in its own bit", 1,
          (fired.num & 0x8) ? 1 : 0);
    check((fired.num & 0x7) == 0, "rcp0 is NOT folded into a saturation bit", 0, fired.num & 0x7);
  }

  // =======================================================================
  // FT033. SLOW-CLEAR AND NO-CLEAR AGREE, FROM DIRTIED REGISTERS.
  // =======================================================================
  // Directive 6.5: run the same legal corpus through both forms with
  // deliberately different initial RF contents. The scratch is POISONED first
  // by running a different program in the same slot that writes large values
  // into registers the measured program reads-before-writing nothing -- so if
  // the fast path were unsound, the second run would see the first's numbers.
  //
  // THE CASE THAT DISCRIMINATES: the two runs must agree AND the counters must
  // show that the two paths were actually different. A test that only compared
  // outputs would pass if `cfg_slow_clear_i` did nothing at all.
  {
    const uint8_t slot = 2;
    set_req_slot(dut, 0, slot);
    set_req_in(dut, 0, 0, 1000);
    set_req_in(dut, 0, 1, 2000);
    set_req_in(dut, 0, 2, 3000);

    // Poison: a run with very different inputs leaves its results in the file.
    set_req_in(dut, 0, 0, 0x0BAD0000);
    set_req_in(dut, 0, 1, 0x0BAD1111);
    set_req_in(dut, 0, 2, 0x0BAD2222);
    (void)run_once(dut, 0);

    set_req_in(dut, 0, 0, 1000);
    set_req_in(dut, 0, 1, 2000);
    set_req_in(dut, 0, 2, 3000);

    // The proof is declared for this slot, so the fast path becomes available.
    //
    // NOTE THE ORDER, WHICH IS THE BLOCK'S OWN LAW AND NOT A CONVENIENCE: every
    // metadata write CLEARS `hdr_loaded`, and the HEADER is what sets it again.
    // So the proof goes in FIRST and the header LAST. Loading the proof after
    // the header would leave the slot un-runnable and the point would answer
    // ST_NO_PROGRAM -- which is what the first version of this test did, and it
    // read as "the fast path was never taken" rather than as a load-order
    // mistake.
    check(load_initproof(dut, slot, true), "FT033 init proof accepted", 1, 1);
    check(load_word(dut, kLdHeader, slot, 0, header_word(4, 4, 0x29, 0x07, 3, kFormCanonical)),
          "FT033 header re-arms the slot", 1, 1);
    const uint32_t fast_before = dut.fast_path_o;
    dut.cfg_slow_clear_i = 0;
    Resp fast = run_once(dut, 0);
    check(fast.got, "FT033 fast path completes", 1, fast.got ? 1 : 0);
    check(dut.fast_path_o == fast_before + 1, "FT033 the fast path was really taken",
          fast_before + 1, dut.fast_path_o);

    const uint32_t slow_before = dut.slow_path_o;
    dut.cfg_slow_clear_i = 1;
    Resp slow = run_once(dut, 0);
    dut.cfg_slow_clear_i = 0;
    check(slow.got, "FT033 slow path completes", 1, slow.got ? 1 : 0);
    check(dut.slow_path_o == slow_before + 1, "FT033 the slow path was really taken",
          slow_before + 1, dut.slow_path_o);

    check(fast.status == slow.status, "FT033 slow-clear and no-clear agree on STATUS", slow.status,
          fast.status);
    check(fast.present == slow.present, "FT033 they agree on the PRESENT mask", slow.present,
          fast.present);
    for (int j = 0; j < kOrdinals; ++j) {
      check(fast.out[j] == slow.out[j], "FT033 they agree on every ordinal",
            static_cast<uint64_t>(slow.out[j]), static_cast<uint64_t>(fast.out[j]));
    }
  }

  // =======================================================================
  // FT028. END HELD OVER SEVERAL CLOCKS PRODUCES ONE TERMINAL EVENT.
  // =======================================================================
  // Not one result per clock. The counter is the instrument: a host that
  // retired per clock would move `runs_o` by more than one for one accepted
  // point, and no output check could see it -- "a test that checks WHAT came
  // out cannot see HOW MANY TIMES the machine did it".
  {
    const uint8_t slot = 2;
    const uint32_t runs_before = dut.runs_o;
    set_req_slot(dut, 0, slot);
    set_req_in(dut, 0, 0, 4);
    set_req_in(dut, 0, 1, 4);
    set_req_in(dut, 0, 2, 4);
    Resp r = run_once(dut, 0);
    check(r.got, "FT028 completes", 1, r.got ? 1 : 0);
    check(dut.runs_o == runs_before + 1, "FT028 exactly ONE terminal event for one point",
          runs_before + 1, dut.runs_o);
  }

  // =======================================================================
  // THE FENCE'S OWN INSTRUMENT, ASSERTED AND SEPARATELY FIREABLE.
  // =======================================================================
  // `late_write_o` counts a granted write for the retired context arriving
  // after the fence released. It must read zero here -- AND a detector reading
  // zero is a claim, not evidence. Its positive control is the committed mutant
  // `tests/mutants/zhao_field_v3_exec_issue_gate_mutant.sv`, which breaks the
  // executor's `!sk_busy_c` issue gate so that a same-context write really can
  // land after END. This assertion is the CORRECT behaviour; the firing lives
  // with the mutant, separately, as the rules require.
  check(dut.late_write_o == 0, "the fence released no point early", 0, dut.late_write_o);
  check(dut.exec_desync_o == 0, "no exec desync", 0, dut.exec_desync_o);
  check(dut.bank_desync_o == 0, "no bank desync", 0, dut.bank_desync_o);
  check(dut.tag_mismatch_o == 0, "no tag mismatch", 0, dut.tag_mismatch_o);
  check(dut.wrong_op_o == 0, "no wrong op", 0, dut.wrong_op_o);
  check(dut.skid_overflow_o == 0, "no skid overflow", 0, dut.skid_overflow_o);

  return zhao::report_and_exit("field_host_v2_directed");
}
