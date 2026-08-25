// field_seq_directed.cpp — the Field IR sequencer, running WHOLE PROGRAMS
// against `zfield::interpret`.
//
// Every other test in this engine checks one op. This one checks the thing that
// runs them: the register file, the instruction walk, the input and output
// lanes, and where a program stops.
//
// The oracle is the shipped interpreter on the SAME program. The harness is the
// instruction memory and the host: it zeroes the file, writes the input lanes,
// starts the walk, and reads the output lanes back -- which is the order the
// reference does it in, and the order is part of the law.
//
// SIX LAWS, each a place an implementation drifts:
//
//   1. THE FILE STARTS AT ZERO and inputs are loaded on top. A register never
//      written and never an input reads zero. Section 2.
//   2. INSTRUCTIONS RUN IN ORDER and results are visible to later ones. A
//      sequencer that reordered or pipelined without a bypass would pass a
//      one-instruction program and fail a chain. Section 3 builds dependency
//      chains up to 24 long.
//   3. OP_END STOPS THE WALK. It does not fall through and it does not write.
//      Section 4 puts live instructions after it and proves they never ran.
//   4. THE LEDGER ACCUMULATES ACROSS THE WHOLE PROGRAM, not per instruction.
//      Section 5 saturates early and checks the lane is still set at the end.
//   5. MULTI-LANE READS COME FROM ADJACENT REGISTERS -- reg[a], reg[a+1],
//      reg[a+2]. Section 6 uses DOT3, which is the only way to see it.
//   6. AN UNSUPPORTED OP IS REFUSED, not skipped and not zero. Section 7.
//
// TWO EQUIVALENT MUTANTS, recorded so they do not read as holes. The write-back
// is guarded by `alu_writes && !alu_is_end && !alu_unsupported`, and the ALU
// clears `writes_o` for EXACTLY those two cases -- OP_END and an unsupported
// op. So `!alu_is_end` and `alu_writes` are individually REDUNDANT: removing
// either one alone changes nothing observable, and both mutations survive.
//
// Removing BOTH does not, and there is a mutation for that
// (`both_write_guards_removed`) so the pair is shown to be load-bearing even
// though neither half is on its own. A redundant guard is not a dead guard.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_seq.h"

#include "zfield/zfield.hpp"
#include "zhao_sim.hpp"

namespace {

using zhao::check;

constexpr int32_t kOne = 1 << 16;

/**
 * Every opcode this test ever issues.
 *
 * The sequencer dispatches all THIRTY Field IR opcodes -- fifteen in the ALU,
 * fifteen in the units -- and nothing checked that the test had actually
 * driven each of them. "All thirty are handled" was a claim about the RTL that
 * no gate enforced, so an op added to the decoder and forgotten here would
 * read as covered. This set plus the check at the end of main() is the gate.
 */
std::set<uint8_t>& opsIssued() {
  static std::set<uint8_t> s;
  return s;
}

/** A program the test builds by hand, in the shape both oracles accept. */
struct Prog {
  std::vector<zfield::Instr> ins;
  std::vector<uint8_t> in_regs;
  std::vector<uint8_t> out_regs;
  // CURVE/DCURVE/SPLINE read prog.tables[imm]; the harness is the table
  // memory just as it is the instruction memory.
  std::vector<zfield::Table> tables;

  void op(uint8_t o, uint8_t dst, uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, uint32_t imm = 0) {
    zfield::Instr i{};
    i.op = o;
    i.dst = dst;
    i.a = a;
    i.b = b;
    i.c = c;
    i.imm = imm;
    opsIssued().insert(o);
    ins.push_back(i);
  }
  void end() { op(zfield::OP_END, 0); }
};

std::vector<int32_t> interp(const Prog& p, const std::vector<int32_t>& in, bool* sat,
                            bool* rcp0 = nullptr) {
  zfield::Decoded d;
  d.profile = 0;
  d.instrs = p.ins;
  d.tables = p.tables;
  for (uint8_t r : p.in_regs) {
    zfield::IoLane l{};
    l.name = "i";
    l.type = 0;
    l.reg = r;
    d.in_lanes.push_back(l);
  }
  for (uint8_t r : p.out_regs) {
    zfield::IoLane l{};
    l.name = "o";
    l.type = 0;
    l.reg = r;
    d.out_lanes.push_back(l);
  }
  std::vector<int32_t> out(p.out_regs.size(), 0);
  const zfield::Status st = zfield::interpret(d, in.data(), in.size(), out.data(), out.size());
  if (sat) *sat = st.sat;
  if (rcp0) *rcp0 = st.rcp0;
  return out;
}

/** The harness IS the instruction memory and the host. */
struct Bench {
  Vzhao_field_seq& dut;
  const Prog* prog = nullptr;

  explicit Bench(Vzhao_field_seq& d) : dut(d) {}

  void present() {
    const size_t pc = dut.pc_o;
    if (prog && pc < prog->ins.size()) {
      const zfield::Instr& i = prog->ins[pc];
      dut.ins_op_i = i.op;
      dut.ins_dst_i = i.dst;
      dut.ins_a_i = i.a;
      dut.ins_b_i = i.b;
      dut.ins_c_i = i.c;
      dut.ins_imm_i = i.imm;
    } else {
      // Off the end: a lawful program never asks, and what it would read is
      // deliberately NOT a harmless zero -- an OP_END here would hide a runaway
      // walk by ending it politely.
      dut.ins_op_i = 0xFF;
      dut.ins_dst_i = 0;
      dut.ins_a_i = 0;
      dut.ins_b_i = 0;
      dut.ins_c_i = 0;
      dut.ins_imm_i = 0;
    }
    // ---- the table memory -------------------------------------------------
    // A REGISTERED read, and the lag is the whole point. The DUT drives
    // `tbl_sel_o`/`tbl_idx_o` during a cycle and the three lanes must answer on
    // the NEXT one -- the curve block's contract, and the M10K rule behind it.
    //
    // AN EARLIER VERSION OF THIS HARNESS ANSWERED COMBINATIONALLY, presenting
    // the entry for the index the DUT was driving RIGHT NOW. That is not more
    // generous, it is wrong: the unit samples in cycle N+1 the entry it asked
    // for in cycle N, so a same-cycle answer hands it whatever index the walk
    // had moved on to. CURVE and SPLINE returned zero and DCURVE -- which needs
    // one entry, not three -- passed, which is exactly the shape that would
    // have read as "the table ops are broken" rather than "the memory model
    // is".
    //
    // So the address is captured at the end of a cycle and answered at the
    // start of the next, which is what a registered read is.
    const uint32_t tsel = tbl_sel_r;
    const uint32_t tidx = tbl_idx_r;
    if (prog && tsel < prog->tables.size()) {
      const zfield::Table& t = prog->tables[tsel];
      dut.tbl_n_i = static_cast<uint8_t>(t.x.size());
      const size_t k = (tidx < t.x.size()) ? tidx : (t.x.empty() ? 0 : t.x.size() - 1);
      dut.tbl_x_i = t.x.empty() ? 0 : static_cast<uint32_t>(t.x[k]);
      dut.tbl_y_i = t.y.empty() ? 0 : static_cast<uint32_t>(t.y[k]);
      dut.tbl_dy_i = t.dy.empty() ? 0 : static_cast<uint32_t>(t.dy[k]);
    } else {
      dut.tbl_n_i = 0;
      dut.tbl_x_i = 0;
      dut.tbl_y_i = 0;
      dut.tbl_dy_i = 0;
    }
    dut.eval();
  }

  // The table memory's address registers: what the DUT asked for during the
  // cycle that just ended, answered at the start of the next one.
  uint32_t tbl_sel_r = 0;
  uint32_t tbl_idx_r = 0;

  void step() {
    present();
    tbl_sel_r = dut.tbl_sel_o;  // latched on this edge, like a real RAM
    tbl_idx_r = dut.tbl_idx_o;
    zhao::tick(dut);
    dut.eval();
  }

  void clear() {
    dut.clear_i = 1;
    dut.eval();
    zhao::tick(dut);
    dut.clear_i = 0;
    dut.eval();
  }

  void write_reg(uint8_t r, int32_t v) {
    dut.rf_we_i = 1;
    dut.rf_waddr_i = r;
    dut.rf_wdata_i = static_cast<uint32_t>(v);
    dut.eval();
    zhao::tick(dut);
    dut.rf_we_i = 0;
    dut.eval();
  }

  int32_t read_reg(uint8_t r) {
    // The register file is block memory now, so the host port is SYNCHRONOUS:
    // the address is presented on one edge and the answer lands on the next.
    // This used to be `eval()` alone, which returned whatever the previous
    // address had selected -- and the symptom was unmistakable once seen, with
    // every chained result reading back as the PREVIOUS instruction's answer.
    // The values were never wrong; only the read protocol moved.
    // Safe to clock here: read_reg is only called once the walk is idle.
    dut.rf_raddr_i = r;
    zhao::tick(dut);
    return static_cast<int32_t>(dut.rf_rdata_o);
  }

  /** Zero, load, run, read back -- the reference's own order. */
  // The WORST number of clocks any single instruction of the last run took,
  // measured from the cycle after the previous retirement (or after start) to
  // the cycle its own `instr_retired_o` pulses. This is the quantity
  // `MAX_OP_CYCLES` bounds and the formal harness proves, so it is measured
  // rather than assumed.
  int max_op_cycles = 0;

  std::vector<int32_t> run(const Prog& p, const std::vector<int32_t>& in, uint8_t* status,
                           int* cycles = nullptr, int* retired = nullptr) {
    prog = &p;
    clear();
    for (size_t i = 0; i < p.in_regs.size() && i < in.size(); ++i) {
      write_reg(p.in_regs[i], in[i]);
    }
    dut.instr_count_i = static_cast<uint8_t>(p.ins.size());
    dut.start_i = 1;
    present();
    zhao::tick(dut);
    dut.start_i = 0;
    dut.eval();

    int n = 0, ret = 0;
    int since = 0;
    max_op_cycles = 0;
    while (!dut.done_o && n < 20000) {
      step();
      ++since;
      if (dut.instr_retired_o) {
        ++ret;
        if (since > max_op_cycles) max_op_cycles = since;
        since = 0;
      }
      ++n;
    }
    if (status) *status = static_cast<uint8_t>(dut.status_o);
    if (cycles) *cycles = n;
    if (retired) *retired = ret;

    std::vector<int32_t> out;
    for (uint8_t r : p.out_regs) out.push_back(read_reg(r));
    return out;
  }
};

void diff(Bench& b, const Prog& p, const std::vector<int32_t>& in, const char* what) {
  bool want_sat = false;
  bool want_rcp0 = false;
  const std::vector<int32_t> want = interp(p, in, &want_sat, &want_rcp0);
  uint8_t status = 0xFF;
  const std::vector<int32_t> got = b.run(p, in, &status);
  const std::string t(what);

  check(status == 0, (t + ": ran to END").c_str(), 0, status);
  for (size_t i = 0; i < want.size(); ++i) {
    char nm[96];
    std::snprintf(nm, sizeof nm, "%s: out[%zu]", what, i);
    check(i < got.size() && got[i] == want[i], nm, static_cast<uint32_t>(want[i]),
          i < got.size() ? static_cast<uint32_t>(got[i]) : 0u);
  }
  // Status.sat is `add || mul || rescale || unit || RCP` -- the reciprocal
  // saturation IS part of it. `rcp0` is NOT, and never has been: it records a
  // reciprocal asked for zero, which has a defined answer. Folding it in would
  // make a defined answer read as an overflow, which is exactly the bug the
  // RING work found and the reason the two are separate fields here too.
  const bool got_sat = b.dut.sat_add_o || b.dut.sat_mul_o || b.dut.sat_rescale_o || b.dut.sat_rcp_o;
  check(got_sat == want_sat, (t + ": Status.sat").c_str(), want_sat ? 1 : 0, got_sat ? 1 : 0);
  const bool got_rcp0 = b.dut.rcp0_o;
  check(got_rcp0 == want_rcp0, (t + ": Status.rcp0").c_str(), want_rcp0 ? 1 : 0, got_rcp0 ? 1 : 0);
}

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    const uint64_t v0 = s;
    s = v0 * 6364136223846793005ULL + 1442695040888963407ULL;
    const uint32_t w = static_cast<uint32_t>(((v0 >> 22) ^ v0) >> 29);
    const uint32_t v = (static_cast<uint32_t>(v0 >> 27) ^ w) * 277803737u;
    return (v >> 22) ^ v;
  }
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0u; }
};

}  // namespace

int main(int argc, char** argv) {
  int random_iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && i + 1 < argc) {
      random_iters = std::atoi(argv[++i]);
    }
  }

  Vzhao_field_seq dut;
  dut.rst_n = 0;
  dut.clear_i = 0;
  dut.start_i = 0;
  dut.rf_we_i = 0;
  dut.rf_raddr_i = 0;
  dut.instr_count_i = 0;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  Bench b(dut);

  // ---- 1. the smallest program that does anything -------------------------
  {
    Prog p;
    p.in_regs = {0, 1};
    p.op(zfield::OP_ADD, 2, 0, 1);
    p.end();
    p.out_regs = {2};
    diff(b, p, {3 * kOne, 4 * kOne}, "1.one add");
    diff(b, p, {INT32_MAX, INT32_MAX}, "1.one add at the rail");
    diff(b, p, {INT32_MIN, -1}, "1.one add at the other rail");
  }

  // ---- 1b. the five ALU ops nothing here had ever issued ------------------
  //
  // The coverage gate at the end of this file found these: SUB, MIN, MAX and
  // ABS lived ONLY in the random pool, so the directed lane never touched
  // them, and CMP was in neither -- dispatched by the ALU, decoded by the
  // reference, and never once executed by this differential.
  {
    Prog p;
    p.in_regs = {0, 1};
    p.op(zfield::OP_SUB, 2, 0, 1);
    p.op(zfield::OP_MIN, 3, 0, 1);
    p.op(zfield::OP_MAX, 4, 0, 1);
    p.op(zfield::OP_ABS, 5, 0);
    p.end();
    p.out_regs = {2, 3, 4, 5};
    diff(b, p, {3 * kOne, 4 * kOne}, "1b.sub/min/max/abs");
    diff(b, p, {-3 * kOne, 4 * kOne}, "1b.sub/min/max/abs negative");
    // the rails, where saturation and ABS(INT32_MIN) are decided
    diff(b, p, {INT32_MIN, 1}, "1b.sub/min/max/abs at the low rail");
    diff(b, p, {INT32_MAX, -1}, "1b.sub/min/max/abs at the high rail");
    diff(b, p, {0, 0}, "1b.sub/min/max/abs at zero");
  }

  // every comparison mode, including the two that differ only on equality
  for (uint32_t mode = 0; mode < 6; ++mode) {
    Prog p;
    p.in_regs = {0, 1};
    p.op(zfield::OP_CMP, 2, 0, 1, 0, mode);
    p.end();
    p.out_regs = {2};
    char nm[64];
    std::snprintf(nm, sizeof nm, "1b.cmp mode %u", mode);
    diff(b, p, {3 * kOne, 4 * kOne}, nm);
    std::snprintf(nm, sizeof nm, "1b.cmp mode %u equal", mode);
    diff(b, p, {4 * kOne, 4 * kOne}, nm);
    std::snprintf(nm, sizeof nm, "1b.cmp mode %u greater", mode);
    diff(b, p, {5 * kOne, 4 * kOne}, nm);
    std::snprintf(nm, sizeof nm, "1b.cmp mode %u signs", mode);
    diff(b, p, {-1, 1}, nm);
  }

  // ---- 2. LAW 1: the file starts at ZERO ---------------------------------
  {
    // reg9 is never written and never an input, so it reads zero -- and MOV
    // from it must produce zero rather than whatever the previous run left.
    Prog p;
    p.in_regs = {0};
    p.op(zfield::OP_MOV, 5, 9);
    p.end();
    p.out_regs = {5};
    diff(b, p, {12345678}, "2.untouched register is zero");

    // Prove it is the CLEAR doing it, not luck: run a program that fills reg5,
    // then run the reading program again and check it still sees zero.
    Prog fill;
    fill.in_regs = {0};
    fill.op(zfield::OP_MOV, 9, 0);
    fill.end();
    fill.out_regs = {9};
    uint8_t st = 0;
    b.run(fill, {0x5A5A5A5A}, &st);
    check(st == 0, "2.the filling program ran", 0, st);
    diff(b, p, {1}, "2.and the next run still starts from zero");
  }

  // ---- 3. LAW 2: instructions run IN ORDER, and results are visible -------
  {
    for (int len : {2, 5, 12, 24}) {
      Prog p;
      p.in_regs = {0};
      // A dependency chain: each instruction reads the one before it.
      uint8_t cur = 0;
      for (int i = 0; i < len; ++i) {
        const uint8_t dst = static_cast<uint8_t>(1 + i);
        p.op(zfield::OP_ADD, dst, cur, 0);
        cur = dst;
      }
      p.end();
      p.out_regs = {cur};
      char nm[64];
      std::snprintf(nm, sizeof nm, "3.chain of %d", len);
      diff(b, p, {kOne}, nm);

      int retired = 0;
      uint8_t st = 0;
      b.run(p, {kOne}, &st, nullptr, &retired);
      std::snprintf(nm, sizeof nm, "3.chain of %d retired %d instructions", len, len);
      check(retired == len, nm, static_cast<uint32_t>(len), static_cast<uint32_t>(retired));
    }
  }

  // ---- 4. LAW 3: OP_END stops the walk -----------------------------------
  // Live instructions after END must never run. If the walk fell through, out
  // would carry their result instead.
  {
    Prog p;
    p.in_regs = {0};
    p.op(zfield::OP_MOV, 4, 0);
    p.end();
    // Unreachable: these would overwrite reg4 with something obvious.
    p.op(zfield::OP_LDC, 4, 0, 0, 0, 0xDEADBEEFu);
    p.op(zfield::OP_LDC, 4, 0, 0, 0, 0xFEEDFACEu);
    p.out_regs = {4};

    // The interpreter stops at END too, so this is a genuine differential --
    // but the value is also checkable by inspection, which is the point.
    uint8_t status = 0;
    const std::vector<int32_t> got = b.run(p, {0x1234}, &status);
    check(status == 0, "4.ran to END", 0, status);
    check(got[0] == 0x1234, "4.the instruction AFTER end never ran", 0x1234u,
          static_cast<uint32_t>(got[0]));

    int retired = 0;
    b.run(p, {0x1234}, &status, nullptr, &retired);
    check(retired == 1, "4.and exactly one instruction retired", 1, static_cast<uint32_t>(retired));

    // END MUST NOT WRITE ANYTHING. Its `dst` field is zero, so a write-back
    // that fired on END would clobber register 0 -- and no program above ever
    // READS register 0 back, which is why a mutation removing both write
    // guards survived untouched. Naming reg0 as an output makes it visible.
    Prog q;
    q.in_regs = {0, 1};
    q.op(zfield::OP_ADD, 2, 0, 1);
    q.end();
    q.out_regs = {2, 0};
    diff(b, q, {0x0BADC0DE, kOne}, "4.END does not write register 0");
    const std::vector<int32_t> got2 = b.run(q, {0x0BADC0DE, kOne}, &status);
    check(got2[1] == 0x0BADC0DE, "4.register 0 survives the END", 0x0BADC0DEu,
          static_cast<uint32_t>(got2[1]));
  }

  // ---- 5. LAW 4: the ledger accumulates across the WHOLE program ----------
  // Saturate on the FIRST instruction, then run many quiet ones. A per-
  // instruction ledger would have forgotten by the end.
  {
    Prog p;
    p.in_regs = {0, 1};
    p.op(zfield::OP_ADD, 2, 0, 1);  // saturates
    for (int i = 0; i < 8; ++i) {
      p.op(zfield::OP_MOV, static_cast<uint8_t>(3 + i), 2);  // quiet
    }
    p.end();
    p.out_regs = {2};
    diff(b, p, {INT32_MAX, INT32_MAX}, "5.early saturation, late finish");

    uint8_t st = 0;
    b.run(p, {INT32_MAX, INT32_MAX}, &st);
    check(dut.sat_add_o, "5.the add lane is STILL set eight instructions later", 1,
          dut.sat_add_o ? 1 : 0);

    // And a quiet program leaves it clear, so "always report" is not a passing
    // answer.
    b.run(p, {kOne, kOne}, &st);
    check(!dut.sat_add_o && !dut.sat_mul_o && !dut.sat_rescale_o,
          "5.a quiet program reports nothing", 0,
          (dut.sat_add_o ? 4u : 0u) | (dut.sat_mul_o ? 2u : 0u) | (dut.sat_rescale_o ? 1u : 0u));
  }

  // ---- 6. LAW 5: multi-lane reads are ADJACENT registers -----------------
  {
    Prog p;
    p.in_regs = {0, 1, 2, 3, 4, 5};
    p.op(zfield::OP_DOT3, 6, 0, 3);  // reg0..2 . reg3..5
    p.end();
    p.out_regs = {6};
    diff(b, p, {kOne, 2 * kOne, 3 * kOne, 4 * kOne, 5 * kOne, 6 * kOne}, "6.dot3");
    diff(b, p, {-kOne, 0, kOne, kOne, kOne, kOne}, "6.dot3 with a negative lane");
    diff(b, p, {INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX},
         "6.dot3 at the rails");

    // DOT2 reads only two lanes from each group; the third must NOT reach it.
    Prog q;
    q.in_regs = {0, 1, 2, 3, 4, 5};
    q.op(zfield::OP_DOT2, 6, 0, 3);
    q.end();
    q.out_regs = {6};
    diff(b, q, {kOne, kOne, 0x7FFFFFFF, kOne, kOne, 0x7FFFFFFF}, "6.dot2 ignores lane 3");
  }

  // ---- 6b. THE `c` OPERAND, which only three ops read --------------------
  // MAD, CLAMP and SELECT are the only ops that read reg[c]. Nothing in
  // sections 1-6 uses one, so the third read port was never checked against
  // anything: a mutation pointing it at reg[a] instead survived the first
  // sweep untouched.
  {
    Prog mad;
    mad.in_regs = {0, 1, 2};
    mad.op(zfield::OP_MAD, 3, 0, 1, 2);
    mad.end();
    mad.out_regs = {3};
    diff(b, mad, {2 * kOne, 3 * kOne, 5 * kOne}, "6b.mad");
    diff(b, mad, {-kOne, kOne, 7 * kOne}, "6b.mad with a negative");
    diff(b, mad, {INT32_MAX, INT32_MAX, INT32_MAX}, "6b.mad at the rails");
    // c distinct from a and b, so pointing the port at the wrong one shows.
    diff(b, mad, {kOne, kOne, 0x4C4C4C4C}, "6b.mad with a distinctive c");

    Prog clamp;
    clamp.in_regs = {0, 1, 2};
    clamp.op(zfield::OP_CLAMP, 3, 0, 1, 2);
    clamp.end();
    clamp.out_regs = {3};
    diff(b, clamp, {5 * kOne, kOne, 3 * kOne}, "6b.clamp above the top");
    diff(b, clamp, {0, kOne, 3 * kOne}, "6b.clamp below the bottom");
    diff(b, clamp, {2 * kOne, kOne, 3 * kOne}, "6b.clamp inside");

    Prog sel;
    sel.in_regs = {0, 1, 2};
    sel.op(zfield::OP_SELECT, 3, 0, 1, 2);
    sel.end();
    sel.out_regs = {3};
    diff(b, sel, {111, 222, 0}, "6b.select takes b when c is zero");
    diff(b, sel, {111, 222, 1}, "6b.select takes a when c is not");
  }

  // ---- 6c. HIGH REGISTERS ------------------------------------------------
  // Every program above lives in the bottom quarter of the file. A clear that
  // only zeroed registers 0..31 survived the first sweep for that reason
  // alone.
  {
    Prog p;
    p.in_regs = {0};
    // Read a register in the TOP half that was never written: it must be zero.
    p.op(zfield::OP_MOV, 40, 63);
    p.op(zfield::OP_ADD, 41, 40, 0);
    p.end();
    p.out_regs = {41, 40};
    diff(b, p, {0x12345678}, "6c.a high register starts at zero");

    // ...and a chain that lives entirely up there.
    Prog q;
    q.in_regs = {0};
    uint8_t cur = 0;
    for (int i = 0; i < 8; ++i) {
      const uint8_t dst = static_cast<uint8_t>(50 + i);
      q.op(zfield::OP_ADD, dst, cur, 0);
      cur = dst;
    }
    q.end();
    q.out_regs = {cur};
    diff(b, q, {kOne}, "6c.a chain in the top half");

    // Prove the clear reaches there: fill a high register, then re-run the
    // program that expects it to be zero.
    Prog fill;
    fill.in_regs = {0};
    fill.op(zfield::OP_MOV, 63, 0);
    fill.end();
    fill.out_regs = {63};
    uint8_t st = 0;
    b.run(fill, {0x7EEDFACE}, &st);
    check(st == 0, "6c.the filling program ran", 0, st);
    diff(b, p, {0x12345678}, "6c.and the clear reached register 63");
  }

  // ---- 7. LAW 6: an unsupported op is REFUSED ----------------------------
  //
  // THIS CHECK USED TO NAME A REAL OP, AND NOW IT CANNOT. It began as OP_RCP,
  // moved to OP_ROT3, then to OP_CURVE, and each move happened because wiring
  // that op made this line fail -- the design working, not churn. With CURVE
  // dispatched there is no real opcode left outside the walk: all sixteen are
  // in. So the real-op half is deleted, as its own note said to do, and what
  // remains is the permanent statement of the law.
  {
    // An opcode that is not an opcode. The decoder would never let this reach
    // hardware; the point is that if one ever did, the walk stops and says so
    // rather than producing a plausible field.
    Prog q;
    q.in_regs = {0};
    q.op(zfield::OP_MOV, 3, 0);
    q.op(0xFE, 4, 3);
    q.end();
    q.out_regs = {3};
    uint8_t st2 = 0;
    int ret2 = 0;
    b.run(q, {kOne}, &st2, nullptr, &ret2);
    check(st2 == 1, "7.an opcode that is not an opcode is REFUSED", 1, st2);
    check(ret2 == 1, "7.and that walk stopped there too", 1, static_cast<uint32_t>(ret2));

    // The refusal must not be a silent zero: the register the refused
    // instruction targeted keeps whatever it had, and the run reports.
    Prog r;
    r.in_regs = {0};
    r.op(zfield::OP_MOV, 3, 0);
    r.op(0xFE, 3, 3);
    r.end();
    r.out_regs = {3};
    uint8_t st3 = 0;
    const std::vector<int32_t> got = b.run(r, {0x1234ABCD}, &st3);
    check(st3 == 1, "7.a refused write still reports", 1, st3);
    check(!got.empty() && got[0] == 0x1234ABCD, "7.and did NOT write the destination", 0x1234ABCDu,
          got.empty() ? 0u : static_cast<uint32_t>(got[0]));
  }

  // ---- 7b. THE COMBINATIONAL UNITS: RCP, SIN, COS -------------------------
  // These three are dispatched to `zhao_field_rcp` and `zhao_field_sin` rather
  // than to the ALU, in the SAME Q_EXEC cycle an ADD uses. Section 7 proves an
  // op the walk does not claim is refused; this section proves these three ARE
  // claimed, and that the claim produces the reference's answer rather than a
  // plausible one.
  {
    Prog rcp;
    rcp.in_regs = {0};
    rcp.op(zfield::OP_RCP, 1, 0);
    rcp.end();
    rcp.out_regs = {1};
    diff(b, rcp, {kOne}, "7b.rcp of one");
    diff(b, rcp, {2 * kOne}, "7b.rcp of two");
    diff(b, rcp, {-3 * kOne}, "7b.rcp of a negative");
    diff(b, rcp, {1}, "7b.rcp of the smallest positive");
    diff(b, rcp, {INT32_MAX}, "7b.rcp at the top rail");
    diff(b, rcp, {INT32_MIN}, "7b.rcp at the bottom rail");

    // THE ZERO CASE, which is the point of the second ledger lane. It has a
    // DEFINED answer, so it must NOT set Status.sat -- only rcp0. `diff`
    // checks both fields separately, so a design that folded them together
    // fails here rather than looking correct.
    diff(b, rcp, {0}, "7b.rcp of zero");

    // ...and a program where a rcp0 happens EARLY and must still be reported
    // at the end, exactly as the saturation lanes are.
    Prog late;
    late.in_regs = {0, 1};
    late.op(zfield::OP_RCP, 2, 1);  // reg[1] is zero -> rcp0
    for (int i = 0; i < 6; ++i) late.op(zfield::OP_ADD, static_cast<uint8_t>(3 + i), 0, 0);
    late.end();
    late.out_regs = {2, 8};
    diff(b, late, {kOne, 0}, "7b.an early rcp0 is still reported at the end");

    // A saturating rcp and a plain add in one program: sat must be the OR.
    Prog both;
    both.in_regs = {0, 1};
    both.op(zfield::OP_RCP, 2, 0);
    both.op(zfield::OP_ADD, 3, 1, 1);
    both.end();
    both.out_regs = {2, 3};
    diff(b, both, {1, INT32_MAX}, "7b.rcp and add saturating together");

    Prog sn;
    sn.in_regs = {0};
    sn.op(zfield::OP_SIN, 1, 0);
    sn.end();
    sn.out_regs = {1};
    Prog cs;
    cs.in_regs = {0};
    cs.op(zfield::OP_COS, 1, 0);
    cs.end();
    cs.out_regs = {1};
    // The quadrant boundaries, where a wrong table index or a wrong sign shows
    // immediately, plus a sweep that no single boundary would catch.
    for (int32_t a : {0, 0x1000, 0x2000, 0x3000, 0x4000, 0x6000, 0x8000, 0xA000, 0xC000, 0xFFFF,
                      0x1234, 0x7FFF}) {
      char nm[64];
      std::snprintf(nm, sizeof nm, "7b.sin(0x%04X)", static_cast<unsigned>(a));
      diff(b, sn, {a}, nm);
      std::snprintf(nm, sizeof nm, "7b.cos(0x%04X)", static_cast<unsigned>(a));
      diff(b, cs, {a}, nm);
    }
    // THE UPPER HALF IS IGNORED, NOT REJECTED. The law is
    // `angle16{(uint16_t)reg[a]}`, so rubbish above bit 15 must not change the
    // answer -- and must not be refused either. A design that fed the whole
    // 32-bit register to the ROM passes every test above and fails this one.
    diff(b, sn, {static_cast<int32_t>(0xDEAD1234)}, "7b.sin ignores the upper half");
    diff(b, cs, {static_cast<int32_t>(0xDEAD1234)}, "7b.cos ignores the upper half");
    diff(b, sn, {static_cast<int32_t>(0xFFFF8000)}, "7b.sin of a negative register");

    // SIN and COS share one unit and are told apart by a single bit. A program
    // that runs both back to back catches a selector latched from the previous
    // instruction rather than this one.
    Prog mix;
    mix.in_regs = {0};
    mix.op(zfield::OP_SIN, 1, 0);
    mix.op(zfield::OP_COS, 2, 0);
    mix.op(zfield::OP_SIN, 3, 0);
    mix.op(zfield::OP_ADD, 4, 1, 2);
    mix.end();
    mix.out_regs = {1, 2, 3, 4};
    diff(b, mix, {0x2000}, "7b.sin and cos alternating");
    diff(b, mix, {0xC000}, "7b.sin and cos alternating, third quadrant");

    // ...and a unit result feeding the ALU, which is the only thing that
    // proves the write-back of a unit lands where the next instruction reads.
    Prog chain;
    chain.in_regs = {0};
    chain.op(zfield::OP_SIN, 1, 0);
    chain.op(zfield::OP_RCP, 2, 1);
    chain.op(zfield::OP_MUL, 3, 1, 2);
    chain.end();
    chain.out_regs = {3, 2, 1};
    diff(b, chain, {0x4000}, "7b.a unit result feeds the next instruction");
    diff(b, chain, {0}, "7b.the same chain through a rcp0");
  }

  // ---- 7c. THE MULTI-CYCLE PATH: LEN2, LEN3, DIST2 -----------------------
  // The first ops that do not finish in Q_EXEC. They hand operands to
  // `zhao_field_len` over a ready/valid handshake, wait an unknown number of
  // clocks, and write back. Everything before this section completed in one
  // cycle, so this is the first exercise of Q_MISS / Q_MWAIT at all.
  //
  // The operands need no extra read state: the three-port walk has already
  // latched a0/a1/a2 and b0/b1 by Q_EXEC, which is exactly LEN3's a..a+2 and
  // DIST2's a..a+1 against b..b+1.
  {
    Prog l2;
    l2.in_regs = {0, 1};
    l2.op(zfield::OP_LEN2, 2, 0);
    l2.end();
    l2.out_regs = {2};
    diff(b, l2, {3 * kOne, 4 * kOne}, "7c.len2 of a 3-4-5 triangle");
    diff(b, l2, {0, 0}, "7c.len2 of the origin");
    diff(b, l2, {kOne, 0}, "7c.len2 on the axis");
    diff(b, l2, {-3 * kOne, -4 * kOne}, "7c.len2 with both negative");
    diff(b, l2, {INT32_MAX, INT32_MAX}, "7c.len2 at the rails");
    diff(b, l2, {INT32_MIN, INT32_MIN}, "7c.len2 at the bottom rails");
    diff(b, l2, {1, 1}, "7c.len2 of the smallest nonzero");

    Prog l3;
    l3.in_regs = {0, 1, 2};
    l3.op(zfield::OP_LEN3, 3, 0);
    l3.end();
    l3.out_regs = {3};
    diff(b, l3, {kOne, 2 * kOne, 2 * kOne}, "7c.len3 of a 1-2-2 vector");
    diff(b, l3, {0, 0, 0}, "7c.len3 of the origin");
    diff(b, l3, {INT32_MAX, INT32_MAX, INT32_MAX}, "7c.len3 at the rails");
    diff(b, l3, {-kOne, kOne, -kOne}, "7c.len3 with mixed signs");

    // DIST2 reads TWO groups -- a..a+1 and b..b+1 -- so it is the case that
    // proves the b-lane operands survived the walk into the slow path.
    Prog d2;
    d2.in_regs = {0, 1, 2, 3};
    d2.op(zfield::OP_DIST2, 4, 0, 2);
    d2.end();
    d2.out_regs = {4};
    diff(b, d2, {0, 0, 3 * kOne, 4 * kOne}, "7c.dist2 from the origin");
    diff(b, d2, {kOne, kOne, kOne, kOne}, "7c.dist2 of a point from itself");
    diff(b, d2, {5 * kOne, 5 * kOne, 2 * kOne, kOne}, "7c.dist2 general");
    diff(b, d2, {INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX}, "7c.dist2 across the whole range");

    // A multi-cycle op FEEDING a single-cycle one, and the reverse. This is
    // what proves the write-back landed where the next instruction reads, and
    // that pc advanced exactly once for the slow instruction.
    Prog chain;
    chain.in_regs = {0, 1};
    chain.op(zfield::OP_LEN2, 2, 0);
    chain.op(zfield::OP_ADD, 3, 2, 0);
    chain.op(zfield::OP_LEN2, 4, 2);  // reads reg2,reg3 -- the previous results
    chain.end();
    chain.out_regs = {4, 3, 2};
    diff(b, chain, {3 * kOne, 4 * kOne}, "7c.a slow result feeds a fast one");
    diff(b, chain, {0, 0}, "7c.the same chain from zero");

    // Two slow ops back to back: the handshake must re-arm, not latch open.
    Prog twice;
    twice.in_regs = {0, 1};
    twice.op(zfield::OP_LEN2, 2, 0);
    twice.op(zfield::OP_LEN3, 3, 0);  // reads reg0..reg2
    twice.end();
    twice.out_regs = {3, 2};
    diff(b, twice, {3 * kOne, 4 * kOne}, "7c.two slow ops back to back");

    // The ledger must cross the slow path: a saturating add BEFORE a slow op
    // has to still be reported after it.
    Prog led;
    led.in_regs = {0, 1};
    led.op(zfield::OP_ADD, 2, 0, 0);  // saturates at the rail
    led.op(zfield::OP_LEN2, 3, 0);
    led.end();
    led.out_regs = {3, 2};
    diff(b, led, {INT32_MAX, kOne}, "7c.a saturation survives the slow path");

    // Retirement is once per instruction, not once per state.
    {
      Prog r;
      r.in_regs = {0, 1};
      r.op(zfield::OP_LEN2, 2, 0);
      r.op(zfield::OP_LEN2, 3, 0);
      r.op(zfield::OP_LEN2, 4, 0);
      r.end();
      r.out_regs = {4};
      uint8_t st = 0;
      int retired = 0;
      b.run(r, {3 * kOne, 4 * kOne}, &st, nullptr, &retired);
      check(st == 0, "7c.the three-slow-op program ran", 0, st);
      // THREE, not four: OP_END does not retire. Q_EXEC's end branch goes
      // straight to Q_DONE without pulsing instr_retired_o, which section 7's
      // refused-op check relies on too. The point being made here is that a
      // SLOW instruction retires ONCE despite occupying four states -- an
      // earlier draft of this check expected four and was wrong about the
      // convention, not about the walk.
      check(retired == 3, "7c.a slow instruction retires once, not once per state", 3,
            static_cast<uint32_t>(retired));
    }
  }

  // ---- 7d. THE MULTI-LANE PATH: NORMALIZE2, NORMALIZE3 -------------------
  // The first ops that write MORE THAN ONE register, so the first exercise of
  // Q_WB1 and Q_WB2 -- the walk that exists because the file has one write
  // port.
  //
  // Section 7c could not test any of that. With only LEN in the multi-cycle
  // group -- one family, one output lane -- seven mutations of the handshake
  // and the walk SURVIVED the sweep: a `multi_op` that ignored the opcode, a
  // pc that advanced early, a per-lane action mistaken for a per-instruction
  // one. None of those were equivalences; the group was simply too narrow to
  // distinguish them. This section is the fix.
  {
    Prog n2;
    n2.in_regs = {0, 1};
    n2.op(zfield::OP_NORMALIZE2, 2, 0);
    n2.end();
    n2.out_regs = {2, 3};  // BOTH lanes, or the walk is untested
    diff(b, n2, {3 * kOne, 4 * kOne}, "7d.normalize2 of a 3-4 vector");
    diff(b, n2, {kOne, 0}, "7d.normalize2 on the axis");
    diff(b, n2, {0, 0}, "7d.normalize2 of the zero vector");
    diff(b, n2, {-kOne, -kOne}, "7d.normalize2 with both negative");
    diff(b, n2, {INT32_MAX, INT32_MAX}, "7d.normalize2 at the rails");
    diff(b, n2, {1, 1}, "7d.normalize2 of the smallest nonzero");

    Prog n3;
    n3.in_regs = {0, 1, 2};
    n3.op(zfield::OP_NORMALIZE3, 3, 0);
    n3.end();
    n3.out_regs = {3, 4, 5};  // all THREE lanes
    diff(b, n3, {kOne, 2 * kOne, 2 * kOne}, "7d.normalize3 of a 1-2-2 vector");
    diff(b, n3, {0, 0, 0}, "7d.normalize3 of the zero vector");
    diff(b, n3, {INT32_MIN, INT32_MAX, kOne}, "7d.normalize3 across the range");
    diff(b, n3, {-kOne, kOne, -kOne}, "7d.normalize3 with mixed signs");

    // THE LANE THE OP DOES NOT OWN MUST NOT MOVE. NORMALIZE2 writes dst and
    // dst+1 and nothing else; if the walk ran one lane too far it would
    // clobber dst+2, which the decoder considers untouched and a later
    // instruction may be reading.
    Prog guard;
    guard.in_regs = {0, 1, 2};
    guard.op(zfield::OP_MOV, 6, 2);         // a live value parked at dst+2
    guard.op(zfield::OP_NORMALIZE2, 4, 0);  // writes 4 and 5 only
    guard.end();
    guard.out_regs = {6, 4, 5};
    diff(b, guard, {3 * kOne, 4 * kOne, 0x5EED1234}, "7d.normalize2 leaves the third lane alone");

    // ...and the same guard one lane further out for NORMALIZE3.
    Prog guard3;
    guard3.in_regs = {0, 1, 2, 3};
    guard3.op(zfield::OP_MOV, 8, 3);
    guard3.op(zfield::OP_NORMALIZE3, 5, 0);  // writes 5, 6, 7
    guard3.end();
    guard3.out_regs = {8, 5, 6, 7};
    diff(b, guard3, {kOne, kOne, kOne, 0x0DDBA11}, "7d.normalize3 leaves the fourth lane alone");

    // A multi-LANE op followed by a single-lane one, and the reverse: proves
    // pc advanced exactly once across the whole walk.
    Prog mix;
    mix.in_regs = {0, 1};
    mix.op(zfield::OP_NORMALIZE2, 2, 0);
    mix.op(zfield::OP_LEN2, 4, 2);  // reads both normalize lanes
    mix.op(zfield::OP_ADD, 5, 4, 0);
    mix.end();
    mix.out_regs = {5, 4, 3, 2};
    diff(b, mix, {3 * kOne, 4 * kOne}, "7d.wide op feeding a narrow one");
    diff(b, mix, {0, 0}, "7d.the same chain from zero");

    // Retirement across a THREE-lane op: four states, one instruction.
    {
      Prog r;
      r.in_regs = {0, 1, 2};
      r.op(zfield::OP_NORMALIZE3, 3, 0);
      r.op(zfield::OP_NORMALIZE3, 6, 0);
      r.end();
      r.out_regs = {6};
      uint8_t st = 0;
      int retired = 0;
      b.run(r, {kOne, 2 * kOne, 2 * kOne}, &st, nullptr, &retired);
      check(st == 0, "7d.the two-wide-op program ran", 0, st);
      check(retired == 2, "7d.a three-lane op still retires once", 2,
            static_cast<uint32_t>(retired));
    }

    // THE LEDGER MUST CROSS THE SLOW PATH, CHECKED PER LANE.
    //
    // `diff()` compares the COLLAPSED Status.sat -- add || mul || rescale ||
    // unit || rcp -- so a slow op that clears the add lane while setting the
    // rescale lane is invisible to it. The sweep proved that: replacing the
    // accumulate with a plain assignment survived every check in 7c, because
    // the LEN2 that followed a saturating ADD set `rescale` and the collapsed
    // value stayed true either way.
    //
    // So this asks the lane directly, the way section 5 does. The ADD saturates
    // the add lane; the LEN2 after it is a clean 3-4-5 triangle that saturates
    // NOTHING. If the slow path assigned instead of accumulating, the add lane
    // would be cleared by an op that never touched it.
    {
      Prog keep;
      keep.in_regs = {0, 1};
      keep.op(zfield::OP_ADD, 2, 0, 0);  // INT32_MAX + INT32_MAX -> saturates
      keep.op(zfield::OP_LEN2, 3, 4);    // reg4/reg5 are zero: len2(0,0) = 0
      keep.end();
      keep.out_regs = {3, 2};
      uint8_t st = 0;
      b.run(keep, {INT32_MAX, kOne}, &st);
      check(st == 0, "7d.the lane-crossing program ran", 0, st);
      check(dut.sat_add_o, "7d.the ADD lane survives a slow op that does not touch it", 1,
            dut.sat_add_o ? 1 : 0);
      check(!dut.sat_mul_o, "7d.and the mul lane was never set", 0, dut.sat_mul_o ? 1 : 0);
    }

    // rcp0 is NORMALIZE2's alone (law 3 of the block). A NORMALIZE3 of the
    // zero vector must NOT set it, and diff() checks that field separately
    // from Status.sat.
    Prog z3;
    z3.in_regs = {0, 1, 2};
    z3.op(zfield::OP_NORMALIZE3, 3, 0);
    z3.end();
    z3.out_regs = {3, 4, 5};
    diff(b, z3, {0, 0, 0}, "7d.normalize3 of zero does not set rcp0");
  }

  // ---- 7e. NOISE2 / RIDGE, ROT2 / ROT3, RING -----------------------------
  // The rest of the ready/valid families. Each reads a DIFFERENT operand
  // shape, and the mapping is the interpreter's -- getting one wrong produces
  // a plausible field and a wrong world, which is precisely what a test that
  // only checked "it returned something" would miss.
  {
    // RIDGE takes its second lane from reg[b], NOT reg[a+1]. That is the one
    // mapping in this group that would silently work-but-wrong if assumed, so
    // the inputs are chosen to make a+1 and b DIFFERENT values.
    Prog rg;
    rg.in_regs = {0, 1, 2};
    rg.op(zfield::OP_RIDGE, 3, 0, 2, 0, 0x1234u);
    rg.end();
    rg.out_regs = {3};
    diff(b, rg, {kOne, 0x0BADF00D, 2 * kOne}, "7e.ridge reads b, not a+1");
    diff(b, rg, {0, 0x5EED, 0}, "7e.ridge at the origin");
    diff(b, rg, {INT32_MIN, 0x1111, INT32_MAX}, "7e.ridge across the range");

    Prog nz;
    nz.in_regs = {0, 1};
    nz.op(zfield::OP_NOISE2, 2, 0, 0, 0, 0xC0FFEEu);
    nz.end();
    nz.out_regs = {2, 3};  // NOISE2 writes TWO lanes
    diff(b, nz, {kOne, 2 * kOne}, "7e.noise2 general");
    diff(b, nz, {0, 0}, "7e.noise2 at the origin");
    diff(b, nz, {INT32_MIN, INT32_MAX}, "7e.noise2 at the rails");

    // The seed is the instruction's imm: two identical coordinates with
    // different seeds must differ, or the imm never reached the unit.
    Prog nz2;
    nz2.in_regs = {0, 1};
    nz2.op(zfield::OP_NOISE2, 2, 0, 0, 0, 0x11111111u);
    nz2.op(zfield::OP_NOISE2, 4, 0, 0, 0, 0x22222222u);
    nz2.end();
    nz2.out_regs = {2, 3, 4, 5};
    diff(b, nz2, {kOne, kOne}, "7e.noise2 seeds reach the unit");

    // ROT2 writes TWO lanes and takes its angle from reg[b].
    Prog r2;
    r2.in_regs = {0, 1, 2};
    r2.op(zfield::OP_ROT2, 3, 0, 2);
    r2.end();
    r2.out_regs = {3, 4};
    diff(b, r2, {kOne, 0, 0}, "7e.rot2 by zero");
    diff(b, r2, {kOne, 0, 0x4000}, "7e.rot2 by a quarter turn");
    diff(b, r2, {3 * kOne, 4 * kOne, 0x2000}, "7e.rot2 by an eighth");
    diff(b, r2, {kOne, kOne, static_cast<int32_t>(0xDEAD8000u)},
         "7e.rot2 ignores the angle's high half");

    // ROT3 writes THREE lanes and takes its axis from the imm.
    Prog r3;
    r3.in_regs = {0, 1, 2, 3};
    r3.op(zfield::OP_ROT3, 4, 0, 3, 0, 0);  // axis X
    r3.end();
    r3.out_regs = {4, 5, 6};
    diff(b, r3, {kOne, 2 * kOne, 3 * kOne, 0x4000}, "7e.rot3 about X");

    Prog r3y;
    r3y.in_regs = {0, 1, 2, 3};
    r3y.op(zfield::OP_ROT3, 4, 0, 3, 0, 1);  // axis Y
    r3y.end();
    r3y.out_regs = {4, 5, 6};
    diff(b, r3y, {kOne, 2 * kOne, 3 * kOne, 0x4000}, "7e.rot3 about Y");

    Prog r3z;
    r3z.in_regs = {0, 1, 2, 3};
    r3z.op(zfield::OP_ROT3, 4, 0, 3, 0, 2);  // axis Z
    r3z.end();
    r3z.out_regs = {4, 5, 6};
    diff(b, r3z, {kOne, 2 * kOne, 3 * kOne, 0x4000}, "7e.rot3 about Z");

    // ...and the axis must actually reach the unit: the same vector about
    // three axes must give three different answers.
    Prog axes;
    axes.in_regs = {0, 1, 2, 3};
    axes.op(zfield::OP_ROT3, 4, 0, 3, 0, 0);
    axes.op(zfield::OP_ROT3, 7, 0, 3, 0, 1);
    axes.op(zfield::OP_ROT3, 10, 0, 3, 0, 2);
    axes.end();
    axes.out_regs = {4, 5, 6, 7, 8, 9, 10, 11, 12};
    diff(b, axes, {kOne, 2 * kOne, 3 * kOne, 0x3000}, "7e.the rot3 axis reaches the unit");

    // ROT2 must NOT write a third lane: the block drives o2 to zero (its law
    // 5), so only the WIDTH protects a live register at dst+2.
    Prog r2g;
    r2g.in_regs = {0, 1, 2, 3};
    r2g.op(zfield::OP_MOV, 8, 3);
    r2g.op(zfield::OP_ROT2, 6, 0, 2);
    r2g.end();
    r2g.out_regs = {8, 6, 7};
    diff(b, r2g, {kOne, kOne, 0x2000, 0x1234ABCD}, "7e.rot2 leaves the third lane alone");

    // RING reads THREE separate registers -- a, b and c -- so it is the only
    // op in this group that exercises the c operand through the slow path.
    Prog rn;
    rn.in_regs = {0, 1, 2};
    rn.op(zfield::OP_RING, 3, 0, 1, 2);
    rn.end();
    rn.out_regs = {3};
    diff(b, rn, {kOne, 0, 2 * kOne}, "7e.ring inside the band");
    diff(b, rn, {3 * kOne, kOne, 2 * kOne}, "7e.ring outside the band");
    diff(b, rn, {kOne, kOne, kOne}, "7e.ring with a degenerate band");
    diff(b, rn, {0, 0, 0}, "7e.ring all zero");
    diff(b, rn, {INT32_MAX, INT32_MIN, INT32_MAX}, "7e.ring at the rails");

    // THE ONE-LANE OPS MUST NOT WRITE A SECOND LANE EITHER.
    //
    // 7d guarded the wide ops at dst+2 and this was missed for the NARROW
    // ones, so two mutations survived the sweep: RIDGE and RING declaring a
    // width of two. Both write a correct value into dst and then clobber
    // dst+1, which no test read. A live value parked there is the whole check.
    Prog rgg;
    rgg.in_regs = {0, 1, 2, 3};
    rgg.op(zfield::OP_MOV, 9, 3);  // live value at RIDGE's dst+1
    rgg.op(zfield::OP_RIDGE, 8, 0, 2, 0, 0x77u);
    rgg.end();
    rgg.out_regs = {9, 8};
    diff(b, rgg, {kOne, 0x1234, 2 * kOne, 0x0C0FFEE}, "7e.ridge leaves the second lane alone");

    Prog rng;
    rng.in_regs = {0, 1, 2, 3};
    rng.op(zfield::OP_MOV, 9, 3);  // live value at RING's dst+1
    rng.op(zfield::OP_RING, 8, 0, 1, 2);
    rng.end();
    rng.out_regs = {9, 8};
    diff(b, rng, {kOne, 0, 2 * kOne, 0x0FACADE}, "7e.ring leaves the second lane alone");

    // ...and the same for the LEN family, which is also one lane. 7c never
    // checked it either.
    Prog lng;
    lng.in_regs = {0, 1, 2};
    lng.op(zfield::OP_MOV, 9, 2);
    lng.op(zfield::OP_LEN2, 8, 0);
    lng.end();
    lng.out_regs = {9, 8};
    diff(b, lng, {3 * kOne, 4 * kOne, 0x0B0BCAFE}, "7e.len2 leaves the second lane alone");

    // A chain across three different families: every one must land where the
    // next reads.
    Prog chain;
    chain.in_regs = {0, 1, 2};
    chain.op(zfield::OP_ROT2, 3, 0, 2);  // 3,4
    chain.op(zfield::OP_LEN2, 5, 3);     // reads 3,4
    chain.op(zfield::OP_RING, 6, 5, 0, 1);
    chain.end();
    chain.out_regs = {6, 5, 4, 3};
    diff(b, chain, {3 * kOne, 4 * kOne, 0x2000}, "7e.rot2 -> len2 -> ring");
  }

  // ---- 7f. CURVE / DCURVE / SPLINE: the family with a table --------------
  // The last three opcodes, and the only ones that read a SECOND memory. The
  // sequencer names a table with the instruction's immediate and the shell
  // answers on the next cycle -- a registered read, the same discipline the
  // instruction memory follows.
  //
  // The harness is that memory. If it answered combinationally the unit would
  // appear to work and the shipped shell would not, which is why the read lead
  // is modelled rather than assumed away.
  {
    // A monotone table with known slopes: y = 2x over [0,4], so dy = 2 in
    // fx16 and the answers are checkable by hand as well as by the oracle.
    zfield::Table t0;
    t0.kind = 0;
    for (int k = 0; k <= 4; ++k) {
      t0.x.push_back(k * kOne);
      t0.y.push_back(2 * k * kOne);
      t0.dy.push_back(2 * kOne);
    }

    // A second table with DIFFERENT contents, so an implementation that
    // ignored the immediate and always read table 0 is visible.
    zfield::Table t1;
    t1.kind = 0;
    for (int k = 0; k <= 4; ++k) {
      t1.x.push_back(k * kOne);
      t1.y.push_back(-3 * k * kOne);
      t1.dy.push_back(-3 * kOne);
    }

    Prog c;
    c.tables = {t0, t1};
    c.in_regs = {0};
    c.op(zfield::OP_CURVE, 1, 0, 0, 0, 0u);  // table 0
    c.end();
    c.out_regs = {1};
    diff(b, c, {0}, "7f.curve at the first knot");
    diff(b, c, {2 * kOne}, "7f.curve mid-table");
    diff(b, c, {4 * kOne}, "7f.curve at the last knot");
    diff(b, c, {kOne + (kOne / 2)}, "7f.curve between knots");
    diff(b, c, {-kOne}, "7f.curve below the table clamps");
    diff(b, c, {9 * kOne}, "7f.curve above the table clamps");
    diff(b, c, {INT32_MIN}, "7f.curve at the bottom rail");
    diff(b, c, {INT32_MAX}, "7f.curve at the top rail");

    // THE IMMEDIATE MUST REACH THE TABLE SELECT. Same input, other table.
    Prog c1;
    c1.tables = {t0, t1};
    c1.in_regs = {0};
    c1.op(zfield::OP_CURVE, 1, 0, 0, 0, 1u);  // table 1
    c1.end();
    c1.out_regs = {1};
    diff(b, c1, {2 * kOne}, "7f.curve reads the table the imm names");

    // ...and both in ONE program, which also proves the selector is held for
    // the whole instruction rather than latched once per run.
    Prog cboth;
    cboth.tables = {t0, t1};
    cboth.in_regs = {0};
    cboth.op(zfield::OP_CURVE, 1, 0, 0, 0, 0u);
    cboth.op(zfield::OP_CURVE, 2, 0, 0, 0, 1u);
    cboth.op(zfield::OP_ADD, 3, 1, 2);
    cboth.end();
    cboth.out_regs = {3, 2, 1};
    diff(b, cboth, {2 * kOne}, "7f.two tables in one program");

    Prog d;
    d.tables = {t0, t1};
    d.in_regs = {0};
    d.op(zfield::OP_DCURVE, 1, 0, 0, 0, 0u);
    d.end();
    d.out_regs = {1};
    diff(b, d, {kOne}, "7f.dcurve returns the slope");
    diff(b, d, {0}, "7f.dcurve at the first knot");
    diff(b, d, {9 * kOne}, "7f.dcurve above the table");
    diff(b, d, {INT32_MIN}, "7f.dcurve at the bottom rail");

    Prog sp;
    sp.tables = {t0, t1};
    sp.in_regs = {0};
    sp.op(zfield::OP_SPLINE, 1, 0, 0, 0, 0u);
    sp.end();
    sp.out_regs = {1};
    diff(b, sp, {0}, "7f.spline at the first knot");
    diff(b, sp, {2 * kOne}, "7f.spline mid-table");
    diff(b, sp, {kOne + (kOne / 2)}, "7f.spline between knots");
    diff(b, sp, {4 * kOne}, "7f.spline at the last knot");
    diff(b, sp, {9 * kOne}, "7f.spline above the table");

    // A table op must not write a second lane either -- the guard the sweep
    // caught missing for the other one-lane families.
    Prog cg;
    cg.tables = {t0};
    cg.in_regs = {0, 1};
    cg.op(zfield::OP_MOV, 6, 1);
    cg.op(zfield::OP_CURVE, 5, 0, 0, 0, 0u);
    cg.end();
    cg.out_regs = {6, 5};
    diff(b, cg, {2 * kOne, 0x0BADCAFE}, "7f.curve leaves the second lane alone");

    // THE OPERAND IS reg[a], AND EVERY CURVE ABOVE USES a = 0.
    //
    // CURVE has ONE source group, so the decoder forces b and c to zero --
    // which means `a0` and `b0` both read register 0 and a mutation feeding
    // the unit from `b` is invisible. It survived the sweep for exactly that
    // reason. Reading from a register that is NOT zero is the whole check.
    Prog cfar;
    cfar.tables = {t0, t1};
    cfar.in_regs = {0, 1};
    cfar.op(zfield::OP_MOV, 7, 1);  // the real input, far from reg0
    cfar.op(zfield::OP_CURVE, 8, 7, 0, 0, 0u);
    cfar.end();
    cfar.out_regs = {8};
    diff(b, cfar, {0, 2 * kOne}, "7f.curve reads reg[a], not reg[b]");
    diff(b, cfar, {4 * kOne, kOne}, "7f.curve reads reg[a] with reg0 distinct");

    // THE LEDGER LANES MUST CROSS THE TABLE PATH.
    //
    // None of the tables above can saturate: the rail inputs clamp to a knot
    // and come back clean, so dropping the curve's sat lanes changed nothing
    // and two mutations survived. This table's values are large enough that
    // the interpolation itself overflows, which is what makes the lanes
    // observable at all.
    zfield::Table tbig;
    tbig.kind = 0;
    tbig.x.push_back(0);
    tbig.y.push_back(INT32_MAX);
    tbig.dy.push_back(INT32_MAX);
    tbig.x.push_back(kOne);
    tbig.y.push_back(INT32_MIN);
    tbig.dy.push_back(INT32_MIN);
    tbig.x.push_back(2 * kOne);
    tbig.y.push_back(INT32_MAX);
    tbig.dy.push_back(INT32_MAX);

    Prog csat;
    csat.tables = {tbig};
    csat.in_regs = {0};
    csat.op(zfield::OP_CURVE, 1, 0, 0, 0, 0u);
    csat.end();
    csat.out_regs = {1};
    diff(b, csat, {kOne / 2}, "7f.curve saturates mid-segment");
    diff(b, csat, {kOne + (kOne / 2)}, "7f.curve saturates in the second segment");
    diff(b, csat, {2 * kOne}, "7f.curve saturates at the last knot");

    // THE ADD LANE NEEDS A TABLE WHOSE X-SPAN SATURATES THE SUBTRACTION.
    //
    // `tbig` above has x = 0, 1, 2 in fx16, so `a - x[i]` never overflows and
    // only the RESCALE lane fires -- which is why dropping the add lane still
    // survived after that table was added. The input is clamped to [x0, xN],
    // so the only way to make the difference saturate is to make the SPAN
    // itself span the whole range.
    zfield::Table twide;
    twide.kind = 0;
    twide.x.push_back(INT32_MIN);
    twide.y.push_back(0);
    twide.dy.push_back(kOne);
    twide.x.push_back(INT32_MAX);
    twide.y.push_back(kOne);
    twide.dy.push_back(kOne);

    Prog cadd;
    cadd.tables = {twide};
    cadd.in_regs = {0};
    cadd.op(zfield::OP_CURVE, 1, 0, 0, 0, 0u);
    cadd.end();
    cadd.out_regs = {1};
    diff(b, cadd, {0}, "7f.curve subtraction saturates at zero");
    diff(b, cadd, {INT32_MAX}, "7f.curve subtraction saturates at the top");
    diff(b, cadd, {kOne}, "7f.curve subtraction saturates near zero");

    Prog dadd;
    dadd.tables = {twide};
    dadd.in_regs = {0};
    dadd.op(zfield::OP_SPLINE, 1, 0, 0, 0, 0u);
    dadd.end();
    dadd.out_regs = {1};
    diff(b, dadd, {0}, "7f.spline over a full-range table");

    Prog ssat;
    ssat.tables = {tbig};
    ssat.in_regs = {0};
    ssat.op(zfield::OP_SPLINE, 1, 0, 0, 0, 0u);
    ssat.end();
    ssat.out_regs = {1};
    diff(b, ssat, {kOne / 2}, "7f.spline saturates mid-segment");
    diff(b, ssat, {kOne + (kOne / 2)}, "7f.spline saturates in the second segment");

    // A table op feeding the rest of the engine, and the reverse.
    Prog chain;
    chain.tables = {t0};
    chain.in_regs = {0, 1};
    chain.op(zfield::OP_CURVE, 2, 0, 0, 0, 0u);
    chain.op(zfield::OP_LEN2, 4, 2);  // reads the curve result and reg3
    chain.op(zfield::OP_DCURVE, 5, 4, 0, 0, 0u);
    chain.end();
    chain.out_regs = {5, 4, 2};
    diff(b, chain, {2 * kOne, kOne}, "7f.curve -> len2 -> dcurve");
  }

  // ---- 8. the liveness bound ---------------------------------------------
  // A program with no reachable END must report rather than hang. The decoder
  // makes this unreachable for lawful programs; the bound is what turns a
  // mis-loaded memory into a status instead of a wedged machine.
  {
    Prog p;
    p.in_regs = {0};
    p.op(zfield::OP_MOV, 2, 0);
    p.op(zfield::OP_MOV, 3, 2);
    // no END
    p.out_regs = {3};
    uint8_t status = 0;
    int cycles = 0;
    b.run(p, {kOne}, &status, &cycles);
    check(status == 2, "8.a program with no END reports PC overrun", 2, status);
    check(cycles < 200, "8.and stops promptly rather than hanging", 1, cycles < 200 ? 1 : 0);
  }

  // ---- 9. interface laws --------------------------------------------------
  {
    Prog p;
    p.in_regs = {0};
    p.op(zfield::OP_MOV, 2, 0);
    p.end();
    p.out_regs = {2};

    uint8_t st = 0;
    b.run(p, {7}, &st);
    check(!dut.busy_o, "9.not busy once done", 0, dut.busy_o ? 1 : 0);

    // Host writes are refused while the walk owns the file. Start a longer
    // program and try to scribble on a register it is about to read.
    Prog q;
    q.in_regs = {0};
    for (int i = 0; i < 10; ++i) q.op(zfield::OP_ADD, static_cast<uint8_t>(1 + i), 0, 0);
    q.end();
    q.out_regs = {10};
    const std::vector<int32_t> want = interp(q, {kOne}, nullptr);

    b.prog = &q;
    b.clear();
    b.write_reg(0, kOne);
    dut.instr_count_i = static_cast<uint8_t>(q.ins.size());
    dut.start_i = 1;
    b.present();
    zhao::tick(dut);
    dut.start_i = 0;
    dut.eval();
    int n = 0;
    while (!dut.done_o && n < 20000) {
      // Scribble every cycle while it runs.
      dut.rf_we_i = 1;
      dut.rf_waddr_i = 0;
      dut.rf_wdata_i = 0xDEADBEEF;
      b.step();
      dut.rf_we_i = 0;
      ++n;
    }
    const int32_t got = b.read_reg(10);
    check(got == want[0], "9.host writes during a run are refused", static_cast<uint32_t>(want[0]),
          static_cast<uint32_t>(got));
  }

  // ---- 10. random programs ------------------------------------------------
  if (random_iters > 0) {
    Prng rng(0x5E97u);
    // The three combinational units are in the pool, so random programs mix
    // them with arithmetic and the ledger has to come out right across a whole
    // program rather than only in the directed cases above.
    const uint8_t pool[] = {zfield::OP_MOV, zfield::OP_ADD, zfield::OP_SUB, zfield::OP_MUL,
                            zfield::OP_MIN, zfield::OP_MAX, zfield::OP_ABS, zfield::OP_RCP,
                            zfield::OP_SIN, zfield::OP_COS, zfield::OP_CMP};
    for (int it = 0; it < random_iters; ++it) {
      Prog p;
      const int n_in = 1 + static_cast<int>(rng.below(4));
      for (int i = 0; i < n_in; ++i) p.in_regs.push_back(static_cast<uint8_t>(i));
      const int len = 1 + static_cast<int>(rng.below(16));
      int defined_hi = n_in - 1;
      for (int i = 0; i < len; ++i) {
        const uint8_t op = pool[rng.below(sizeof pool / sizeof pool[0])];
        const uint8_t dst = static_cast<uint8_t>(defined_hi + 1);
        if (dst >= 60) break;
        const uint8_t a = static_cast<uint8_t>(rng.below(defined_hi + 1));
        const uint8_t bb = static_cast<uint8_t>(rng.below(defined_hi + 1));
        // MOV and ABS read one operand; the decoder requires unused operand
        // fields to be ZERO, so a stray `b` would be a malformed program.
        const bool unary = (op == zfield::OP_MOV || op == zfield::OP_ABS || op == zfield::OP_RCP ||
                            op == zfield::OP_SIN || op == zfield::OP_COS);
        // CMP's imm chooses the comparison, and the decoder requires it in
        // range; every other op in this pool takes imm zero.
        const uint32_t imm = (op == zfield::OP_CMP) ? rng.below(6) : 0u;
        p.op(op, dst, a, unary ? 0 : bb, 0, imm);
        defined_hi = dst;
      }
      p.end();
      p.out_regs = {static_cast<uint8_t>(defined_hi)};
      std::vector<int32_t> in;
      for (int i = 0; i < n_in; ++i) {
        // Full-range words almost never hit the interesting reciprocal inputs,
        // so one input in eight is steered to a small magnitude or to ZERO.
        // Without this the rcp0 lane is exercised only by the directed cases
        // and the random lane silently proves nothing about it.
        const uint32_t r = rng.next();
        if ((r & 7u) == 0u) {
          in.push_back(static_cast<int32_t>(r >> 29) - 3);
        } else {
          in.push_back(static_cast<int32_t>(r));
        }
      }
      char nm[64];
      std::snprintf(nm, sizeof nm, "10.random[%d]", it);
      diff(b, p, in, nm);
    }
    std::printf("random: %d programs\n", random_iters);
  }

  // ---- 11. every opcode the sequencer claims to dispatch was issued --------
  {
    struct OpName {
      uint8_t op;
      const char* name;
    };
    static const OpName kAll[] = {
        {zfield::OP_END, "END"},
        {zfield::OP_MOV, "MOV"},
        {zfield::OP_LDC, "LDC"},
        {zfield::OP_ADD, "ADD"},
        {zfield::OP_SUB, "SUB"},
        {zfield::OP_MUL, "MUL"},
        {zfield::OP_MAD, "MAD"},
        {zfield::OP_MIN, "MIN"},
        {zfield::OP_MAX, "MAX"},
        {zfield::OP_ABS, "ABS"},
        {zfield::OP_CLAMP, "CLAMP"},
        {zfield::OP_SELECT, "SELECT"},
        {zfield::OP_CMP, "CMP"},
        {zfield::OP_DOT2, "DOT2"},
        {zfield::OP_DOT3, "DOT3"},
        {zfield::OP_LEN2, "LEN2"},
        {zfield::OP_LEN3, "LEN3"},
        {zfield::OP_DIST2, "DIST2"},
        {zfield::OP_NORMALIZE2, "NORMALIZE2"},
        {zfield::OP_NORMALIZE3, "NORMALIZE3"},
        {zfield::OP_RCP, "RCP"},
        {zfield::OP_SIN, "SIN"},
        {zfield::OP_COS, "COS"},
        {zfield::OP_CURVE, "CURVE"},
        {zfield::OP_SPLINE, "SPLINE"},
        {zfield::OP_NOISE2, "NOISE2"},
        {zfield::OP_DCURVE, "DCURVE"},
        {zfield::OP_RING, "RING"},
        {zfield::OP_RIDGE, "RIDGE"},
        {zfield::OP_ROT2, "ROT2"},
        {zfield::OP_ROT3, "ROT3"},
    };
    for (const OpName& o : kAll) {
      char nm[64];
      std::snprintf(nm, sizeof nm, "11.coverage: %s issued", o.name);
      zhao::check(opsIssued().count(o.op) == 1, nm, 1, static_cast<int>(opsIssued().count(o.op)));
    }
    // opsIssued() can exceed the required set: some cases issue a deliberately
    // INVALID opcode to prove the unsupported path, and that is not a gap.
    std::printf("coverage: all %d required opcodes issued (%d distinct seen)\n",
                static_cast<int>(sizeof(kAll) / sizeof(kAll[0])),
                static_cast<int>(opsIssued().size()));
  }

  // ---- 12. THE LATENCY TABLE, MEASURED ------------------------------------
  // The engine has ONE multiplier now, so what each op costs is a property of
  // the schedule rather than of its silicon, and two numbers in the contract
  // depend on it:
  //
  //   * MUL, MAD, DOT2 and DOT3 still retire in SIX CLOCKS. That is the whole
  //     reason the first operand group is read in Q_LATCH: with a two-cycle
  //     lane it puts DOT3's third product in Q_EXEC. If a future change moves
  //     the issue back to Q_RD1, DOT3 silently costs seven and this catches it.
  //   * NO op exceeds MAX_OP_CYCLES, which is what tests/formal/field_seq_bound
  //     proves for an arbitrary instruction memory. The formal lane proves the
  //     bound holds; this section is where the MARGIN is visible, so that a
  //     bound quietly grown to fit a regression is a diff rather than a habit.
  {
    const int kMaxOpCycles = 80;  // zhao_field_seq_pkg::MAX_OP_CYCLES

    struct Case {
      const char* name;
      uint8_t op;
      bool simple;  // must retire in exactly six clocks
      int lanes;    // how many registers it writes
    };
    // Operand registers are loaded as input lanes so nothing is read before it
    // is written, and every destination is well clear of every source.
    static const Case kCases[] = {
        {"MOV", zfield::OP_MOV, true, 1},
        {"ADD", zfield::OP_ADD, true, 1},
        {"MUL", zfield::OP_MUL, true, 1},
        {"MAD", zfield::OP_MAD, true, 1},
        {"DOT2", zfield::OP_DOT2, true, 1},
        {"DOT3", zfield::OP_DOT3, true, 1},
        {"SIN", zfield::OP_SIN, true, 1},
        {"COS", zfield::OP_COS, true, 1},
        {"RCP", zfield::OP_RCP, false, 1},
        {"LEN2", zfield::OP_LEN2, false, 1},
        {"LEN3", zfield::OP_LEN3, false, 1},
        {"DIST2", zfield::OP_DIST2, false, 1},
        {"NORMALIZE2", zfield::OP_NORMALIZE2, false, 2},
        {"NORMALIZE3", zfield::OP_NORMALIZE3, false, 3},
        {"NOISE2", zfield::OP_NOISE2, false, 2},
        {"RIDGE", zfield::OP_RIDGE, false, 1},
        {"ROT2", zfield::OP_ROT2, false, 2},
        {"ROT3", zfield::OP_ROT3, false, 3},
        {"RING", zfield::OP_RING, false, 1},
        {"CURVE", zfield::OP_CURVE, false, 1},
        {"DCURVE", zfield::OP_DCURVE, false, 1},
        {"SPLINE", zfield::OP_SPLINE, false, 1},
    };

    // `kind` MUST be set even though `zfield::interpret` never reads it.
    //
    // The interpreter dispatches CURVE / DCURVE / SPLINE on the OPCODE, so a
    // table's `kind` byte is inert on this path -- which is exactly why leaving
    // it indeterminate went unnoticed until cppcheck said so. It is still a
    // defect and not a nit: `zfield::Decoded` is what a DECODED program is, and
    // the decoder validates this field (`kind > 1` is rejected outright, and
    // `kind == 1` additionally requires uniform x spacing). A test that hands
    // `interpret` a struct the decoder would refuse is not running a program,
    // it is running something that happens to agree.
    //
    // 1 = spline, and these tables really are uniformly spaced (i * kOne/4), so
    // the stricter of the two kinds is the honest label and it is the one the
    // SPLINE cases need.
    zfield::Table tbl;
    tbl.kind = 1;
    for (int i = 0; i < 8; ++i) {
      tbl.x.push_back(static_cast<int32_t>(i) * (kOne / 4));
      tbl.y.push_back(static_cast<int32_t>(i) * 1234 - 3000);
      tbl.dy.push_back(kOne / 3);
    }

    int worst = 0;
    const char* worst_name = "";
    std::printf("latency (clocks per instruction, measured):\n");
    for (const Case& cs : kCases) {
      Prog p;
      p.in_regs = {0, 1, 2, 3, 4, 5, 6};
      p.tables.push_back(tbl);
      // a = r0..r2, b = r3..r5, c = r6, dst = r16..r18.
      p.op(cs.op, 16, 0, 3, 6, (cs.op == zfield::OP_ROT3) ? 1u : 0u);
      p.end();
      for (int k = 0; k < cs.lanes; ++k) p.out_regs.push_back(static_cast<uint8_t>(16 + k));

      uint8_t status = 0xFF;
      b.run(p, {kOne, kOne * 2, kOne * 3, kOne / 2, kOne, kOne * 5, kOne * 9}, &status);
      const int got = b.max_op_cycles;
      std::printf("    %-12s %3d\n", cs.name, got);

      char nm[96];
      std::snprintf(nm, sizeof nm, "12.%s ran to END", cs.name);
      check(status == 0, nm, 0, status);
      if (cs.simple) {
        // SEVEN, not six. The register file became block memory, so a read is
        // answered one edge after its address and every operand group lands a
        // state later -- Q_LATCH now only ADDRESSES group 0, and a fourth read
        // state (Q_RD3) catches the last one. This one extra clock was
        // predicted in the run's SPEC before the RTL was written, which is what
        // makes updating the number here a recorded consequence rather than a
        // test bent to fit a result. Every VALUE is unchanged and still
        // bit-identical to zfield::interpret.
        std::snprintf(nm, sizeof nm, "12.%s retires in seven clocks", cs.name);
        check(got == 7, nm, 7, static_cast<uint32_t>(got));
      }
      std::snprintf(nm, sizeof nm, "12.%s within MAX_OP_CYCLES", cs.name);
      check(got <= kMaxOpCycles, nm, 1, got <= kMaxOpCycles ? 1 : 0);
      if (got > worst) {
        worst = got;
        worst_name = cs.name;
      }
    }
    std::printf("    worst op: %s at %d clocks (MAX_OP_CYCLES = %d)\n", worst_name, worst,
                kMaxOpCycles);
    // A bound with no margin left is a bound about to be edited. This is not a
    // correctness law, it is a tripwire on the habit.
    check(worst <= kMaxOpCycles, "12.the worst op fits MAX_OP_CYCLES", 1,
          worst <= kMaxOpCycles ? 1 : 0);
  }

  // ---- 13. CROSS-OP CONTAMINATION: alone versus interleaved ---------------
  // THE DEFECT THIS SECTION EXISTS FOR IS ONE THE OLD DESIGN COULD NOT HAVE
  // HAD. When every op owned its own multiplier and its own accumulator,
  // nothing an op left behind could reach another one. They now share a 66-bit
  // product register, a wide accumulator, an integer square root, a sine table
  // and a reciprocal -- and a leftover in any of them is invisible to every
  // test that runs one op at a time, which is every other section of this file
  // and every block-level differential in the tree.
  //
  // So: run each op ALONE, then run hostile sequences of the same ops, and
  // require every answer AND EVERY SATURATION LEDGER LANE to equal what the
  // isolated run produced. The ledger is the half that matters most -- a shared
  // accumulator not cleared between ops can produce the right number and the
  // wrong `mul` lane, and Status.sat collapses all five into one bit, so the
  // five are compared separately here.
  //
  // The sequences are chosen to put the deepest sharers next to each other:
  // NORMALIZE (root + reciprocal walk + three lane products) beside RCP (the
  // split correction product) beside RING (nine products and TWO reciprocal
  // calls) beside DOT3 (the read-walk accumulator) beside NOISE (six products
  // through the same lane).
  {
    struct Step {
      const char* name;
      uint8_t op;
      uint8_t dst;
      uint8_t a, b, c;
      uint32_t imm;
      int lanes;
    };
    // Sources are input lanes r0..r15, loaded before the run. Destinations are
    // r20 upward, three apart, so no op can write into another's answer even if
    // the width logic were wrong.
    static const Step kSteps[] = {
        {"NORMALIZE3", zfield::OP_NORMALIZE3, 20, 0, 0, 0, 0, 3},
        {"RCP", zfield::OP_RCP, 24, 3, 0, 0, 0, 1},
        {"RING", zfield::OP_RING, 28, 4, 5, 6, 0, 1},
        {"DOT3", zfield::OP_DOT3, 32, 0, 8, 0, 0, 1},
        {"NOISE2", zfield::OP_NOISE2, 36, 11, 0, 0, 0x5EEDu, 2},
        {"ROT3", zfield::OP_ROT3, 40, 13, 7, 0, 2u, 3},
        {"LEN3", zfield::OP_LEN3, 44, 0, 0, 0, 0, 1},
        {"SPLINE", zfield::OP_SPLINE, 48, 3, 0, 0, 0u, 1},
        {"MUL", zfield::OP_MUL, 52, 3, 4, 0, 0, 1},
        {"NORMALIZE2", zfield::OP_NORMALIZE2, 56, 8, 0, 0, 0, 2},
    };
    const int kN = static_cast<int>(sizeof(kSteps) / sizeof(kSteps[0]));

    zfield::Table tbl;
    tbl.kind = 1;  // spline; see the note in section 12
    for (int i = 0; i < 8; ++i) {
      tbl.x.push_back(static_cast<int32_t>(i) * (kOne / 4));
      tbl.y.push_back(static_cast<int32_t>(i) * 4321 - 9000);
      tbl.dy.push_back(kOne / 3);
    }

    const std::vector<uint8_t> srcs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    // Values chosen so the deep ops actually do work: a non-degenerate ring, a
    // vector that needs normalising, an angle that is not a quadrant boundary,
    // and a reciprocal operand small enough to exercise the correction.
    const std::vector<int32_t> vals = {
        kOne * 3, -kOne * 4, kOne * 12, kOne / 7,   kOne * 2, kOne * 5, kOne * 9,  0x2A7Fu,
        -kOne,    kOne * 6,  kOne / 3,  -kOne * 11, kOne * 8, kOne * 2, -kOne * 3, kOne * 4};

    struct Lanes {
      bool add = false, mul = false, rescale = false, rcp = false, rcp0 = false;
    };

    // ---- each op ALONE -----------------------------------------------------
    std::vector<std::vector<int32_t>> alone(kN);
    std::vector<Lanes> alone_lanes(kN);
    for (int i = 0; i < kN; ++i) {
      const Step& st = kSteps[i];
      Prog p;
      p.in_regs = srcs;
      p.tables.push_back(tbl);
      p.op(st.op, st.dst, st.a, st.b, st.c, st.imm);
      p.end();
      for (int k = 0; k < st.lanes; ++k) p.out_regs.push_back(static_cast<uint8_t>(st.dst + k));
      uint8_t status = 0xFF;
      alone[i] = b.run(p, vals, &status);
      alone_lanes[i].add = dut.sat_add_o != 0;
      alone_lanes[i].mul = dut.sat_mul_o != 0;
      alone_lanes[i].rescale = dut.sat_rescale_o != 0;
      alone_lanes[i].rcp = dut.sat_rcp_o != 0;
      alone_lanes[i].rcp0 = dut.rcp0_o != 0;
      char nm[96];
      std::snprintf(nm, sizeof nm, "13.%s alone ran to END", st.name);
      check(status == 0, nm, 0, status);
      // The isolated answer is itself checked against the oracle, so a shared
      // sequence agreeing with a wrong isolated run is not a pass.
      std::snprintf(nm, sizeof nm, "13.%s alone", st.name);
      diff(b, p, vals, nm);
    }

    // ---- the same ops, INTERLEAVED -----------------------------------------
    // Two orders. Reversing them moves every op to a different neighbour, and a
    // leftover that only travels forwards would survive one order and not the
    // other.
    for (int pass = 0; pass < 2; ++pass) {
      const bool reversed = (pass == 1);
      Prog p;
      p.in_regs = srcs;
      p.tables.push_back(tbl);
      std::vector<int> order;
      for (int i = 0; i < kN; ++i) order.push_back(reversed ? (kN - 1 - i) : i);
      for (int i : order) {
        const Step& st = kSteps[i];
        p.op(st.op, st.dst, st.a, st.b, st.c, st.imm);
      }
      p.end();
      for (int i = 0; i < kN; ++i) {
        for (int k = 0; k < kSteps[i].lanes; ++k) {
          p.out_regs.push_back(static_cast<uint8_t>(kSteps[i].dst + k));
        }
      }

      uint8_t status = 0xFF;
      const std::vector<int32_t> got = b.run(p, vals, &status);
      Lanes seq;
      seq.add = dut.sat_add_o != 0;
      seq.mul = dut.sat_mul_o != 0;
      seq.rescale = dut.sat_rescale_o != 0;
      seq.rcp = dut.sat_rcp_o != 0;
      seq.rcp0 = dut.rcp0_o != 0;

      char nm[128];
      std::snprintf(nm, sizeof nm, "13.%s sequence ran to END", reversed ? "reversed" : "forward");
      check(status == 0, nm, 0, status);

      size_t at = 0;
      for (int i = 0; i < kN; ++i) {
        for (int k = 0; k < kSteps[i].lanes; ++k, ++at) {
          std::snprintf(nm, sizeof nm, "13.%s %s lane %d equals its isolated answer",
                        reversed ? "reversed" : "forward", kSteps[i].name, k);
          check(
              at < got.size() && static_cast<size_t>(k) < alone[i].size() && got[at] == alone[i][k],
              nm, static_cast<uint32_t>(alone[i][k]),
              at < got.size() ? static_cast<uint32_t>(got[at]) : 0u);
        }
      }

      // The ledger is sticky across the whole program, so the sequence's lanes
      // must be exactly the OR of the isolated runs' lanes -- no more (a lane
      // that only fires because of a leftover) and no less (a lane lost because
      // a shared register was overwritten before it was read).
      Lanes want;
      for (int i = 0; i < kN; ++i) {
        want.add |= alone_lanes[i].add;
        want.mul |= alone_lanes[i].mul;
        want.rescale |= alone_lanes[i].rescale;
        want.rcp |= alone_lanes[i].rcp;
        want.rcp0 |= alone_lanes[i].rcp0;
      }
      const char* tag = reversed ? "reversed" : "forward";
      std::snprintf(nm, sizeof nm, "13.%s ledger add", tag);
      check(seq.add == want.add, nm, want.add ? 1 : 0, seq.add ? 1 : 0);
      std::snprintf(nm, sizeof nm, "13.%s ledger mul", tag);
      check(seq.mul == want.mul, nm, want.mul ? 1 : 0, seq.mul ? 1 : 0);
      std::snprintf(nm, sizeof nm, "13.%s ledger rescale", tag);
      check(seq.rescale == want.rescale, nm, want.rescale ? 1 : 0, seq.rescale ? 1 : 0);
      std::snprintf(nm, sizeof nm, "13.%s ledger rcp", tag);
      check(seq.rcp == want.rcp, nm, want.rcp ? 1 : 0, seq.rcp ? 1 : 0);
      std::snprintf(nm, sizeof nm, "13.%s ledger rcp0", tag);
      check(seq.rcp0 == want.rcp0, nm, want.rcp0 ? 1 : 0, seq.rcp0 ? 1 : 0);

      // And the whole sequence against the interpreter, which is what makes the
      // agreement above mean the RIGHT answer rather than a consistent one.
      std::snprintf(nm, sizeof nm, "13.%s sequence", tag);
      diff(b, p, vals, nm);
    }

    // ---- the adversarial pair: the same op twice, back to back -------------
    // A shared accumulator that is ADDED to rather than LOADED shows up here
    // even when every op in the sequence is different does not: two identical
    // DOT3s in a row must give the same answer twice.
    {
      Prog p;
      p.in_regs = srcs;
      p.tables.push_back(tbl);
      p.op(zfield::OP_DOT3, 20, 0, 8, 0, 0);
      p.op(zfield::OP_DOT3, 24, 0, 8, 0, 0);
      p.op(zfield::OP_DOT3, 28, 0, 8, 0, 0);
      p.end();
      p.out_regs = {20, 24, 28};
      uint8_t status = 0xFF;
      const std::vector<int32_t> got = b.run(p, vals, &status);
      check(status == 0, "13.repeated DOT3 ran to END", 0, status);
      check(got.size() == 3 && got[0] == got[1], "13.DOT3 twice gives the same answer",
            static_cast<uint32_t>(got.size() > 0 ? got[0] : 0),
            static_cast<uint32_t>(got.size() > 1 ? got[1] : 0));
      check(got.size() == 3 && got[1] == got[2], "13.DOT3 three times gives the same answer",
            static_cast<uint32_t>(got.size() > 1 ? got[1] : 0),
            static_cast<uint32_t>(got.size() > 2 ? got[2] : 0));
      diff(b, p, vals, "13.repeated DOT3");
    }
  }

  // ---- 14. WHEN A LONG OPERATION COMMITS ----------------------------------
  // THE GAP THIS CLOSES was found by the mutation sweep, not by review. M20
  // deletes `&& !multi_op` from the write-back guard, so a multi-cycle op ALSO
  // writes reg[dst] in Q_EXEC -- forty clocks before it has an answer -- and
  // every test in this file passed anyway.
  //
  // It passed because the early write is immediately WRONG but eventually
  // OVERWRITTEN: Q_MWAIT writes the real lane-0 value to the same register, and
  // nothing in between reads it. The decoder guarantees `dst` does not overlap
  // this instruction's own sources, and the next instruction does not read the
  // file until after retirement. So every ANSWER stays correct and the defect
  // is invisible to a test that only looks at answers.
  //
  // That is a real hole rather than an equivalence, and the reason it matters
  // is sequencing: once operations take tens of clocks, WHEN a result becomes
  // visible is part of the contract, not an implementation detail. A future
  // reader of the register file mid-run -- a debug port, a second engine, an
  // interrupted run -- would see a zero that the design never promised.
  //
  // So this section watches the file DURING the walk. `rf_rdata_o` is
  // combinational and is readable at any time; reading it disturbs nothing,
  // because `rf_we_i` stays low and the walk owns the write port.
  //
  // The sentinel is what makes it observable. `clear()` leaves the file at
  // zero and M20's early write is also zero, so a cleared register cannot tell
  // them apart. The program therefore WRITES a distinctive value into the
  // destination first, and the law is: that value survives, untouched, until
  // the long operation retires.
  {
    // COMMITMENT IS NOT ATOMIC FOR A MULTI-LANE OP, and the law has to say so
    // or it is wrong rather than strict. The register file has ONE write port:
    // lane 0 lands on the cycle the unit's answer is accepted (Q_MWAIT), lane 1
    // on the next (Q_WB1), lane 2 on the one after (Q_WB2), and
    // `instr_retired_o` pulses with the LAST of them. So reg[dst] legitimately
    // changes `lanes - 1` cycles before retirement.
    //
    // The law is therefore: reg[dst] holds its previous value until at most
    // `lanes - 1` cycles before the operation retires. For a single-lane op
    // that is exact -- the sentinel must survive to the retiring edge itself --
    // and single-lane ops are what pin M20, which writes lane 0 in Q_EXEC, tens
    // of clocks early.
    struct Case {
      const char* name;
      uint8_t op;
      uint8_t a, b;
      int lanes;
    };
    // One per multi-cycle family, so the guard is pinned for each dispatch arm
    // rather than for whichever one happened to be tested.
    static const Case kCases[] = {
        {"LEN2", zfield::OP_LEN2, 0, 0, 1},
        {"RCP", zfield::OP_RCP, 0, 0, 1},
        {"NORMALIZE2", zfield::OP_NORMALIZE2, 0, 0, 2},
        {"NOISE2", zfield::OP_NOISE2, 0, 0, 2},
        {"ROT2", zfield::OP_ROT2, 0, 3, 2},
        {"RING", zfield::OP_RING, 0, 3, 1},
        {"CURVE", zfield::OP_CURVE, 0, 0, 1},
    };

    zfield::Table tbl;
    tbl.kind = 1;  // spline; see the note in section 12
    for (int i = 0; i < 8; ++i) {
      tbl.x.push_back(static_cast<int32_t>(i) * (kOne / 4));
      tbl.y.push_back(static_cast<int32_t>(i) * 777 - 1500);
      tbl.dy.push_back(kOne / 3);
    }

    const int32_t kSentinel = static_cast<int32_t>(0x5A5A5A5A);

    for (const Case& cs : kCases) {
      Prog p;
      p.in_regs = {0, 1, 2, 3, 4, 5};
      p.tables.push_back(tbl);
      // Instruction 0 plants the sentinel in the destination. Instruction 1 is
      // the long op writing the same register -- legal, because `dst` overlaps
      // neither an input lane nor its own sources.
      p.op(zfield::OP_LDC, 20, 0, 0, 0, static_cast<uint32_t>(kSentinel));
      p.op(cs.op, 20, cs.a, cs.b, 5, 0);
      p.end();
      p.out_regs = {20};

      // Drive the run by hand so the file can be watched every cycle.
      b.prog = &p;
      b.clear();
      const int32_t in[6] = {kOne * 2, kOne * 3, kOne * 5, 0x1234, kOne, kOne * 7};
      for (size_t i = 0; i < p.in_regs.size(); ++i) b.write_reg(p.in_regs[i], in[i]);
      dut.instr_count_i = static_cast<uint8_t>(p.ins.size());
      dut.start_i = 1;
      b.present();
      zhao::tick(dut);
      dut.start_i = 0;
      dut.eval();

      int retires = 0;
      bool planted = false;
      bool seen_sentinel = false;
      int planted_cycle = -1;
      bool changed_early = false;
      int change_cycle = -1;
      int retire_cycle = -1;
      for (int n = 0; n < 4000 && !dut.done_o; ++n) {
        // Watch reg[20] WITHOUT clocking: read_reg() now advances the clock
        // (the file is block memory), and calling it inside this loop ticked
        // twice per iteration and swallowed instr_retired_o pulses. step() does
        // not touch rf_raddr_i, so holding the address and reading the port
        // directly gives the same observation with no extra edge.
        dut.rf_raddr_i = 20;
        b.step();
        const int32_t now = static_cast<int32_t>(dut.rf_rdata_o);
        if (dut.instr_retired_o) {
          ++retires;
          if (retires == 1) {
            planted = true;
            planted_cycle = n;
          }                                    // LDC committed
          if (retires == 2) retire_cycle = n;  // the long op has committed
        }
        // Between the sentinel landing and the long op retiring, reg[20] is the
        // sentinel and nothing else. `retires == 1` is exactly that window.
        //
        // THE START OF THE WINDOW IS THE SENTINEL'S ARRIVAL, OBSERVED -- not an
        // offset from the retire pulse. The read port lags the write, and how
        // MUCH it lags is a property of the write path that every timing wave
        // changes. This test has already been fooled twice by assuming a fixed
        // offset: once at `change_cycle == 6` when the register file became
        // block memory, and again at 7 when a wave-4 attempt registered the
        // write-back. Both times EVERY opcode reported the same early cycle,
        // against expected values ranging 0x16..0x48.
        //
        // A CONSTANT WHERE THERE SHOULD BE VARIANCE IS AN OBSERVER ARTEFACT.
        // Waiting for the sentinel to actually appear makes the check immune to
        // the lag entirely, so the next wave that moves write timing is measured
        // rather than mis-blamed.
        if (planted && !seen_sentinel && now == kSentinel) seen_sentinel = true;
        if (seen_sentinel && retires == 1 && now != kSentinel && !changed_early) {
          changed_early = true;
          change_cycle = n;
        }
      }

      // The write-back walk is allowed to touch reg[dst] at most `lanes - 1`
      // cycles before retirement, and not one cycle earlier.
      const int slack = cs.lanes - 1;
      const bool too_early =
          changed_early && (retire_cycle < 0 || change_cycle < (retire_cycle - slack));
      char nm[128];
      std::snprintf(nm, sizeof nm, "14.%s: reg[dst] is not written before the write-back walk",
                    cs.name);
      check(!too_early, nm, static_cast<uint32_t>(retire_cycle - slack),
            too_early ? static_cast<uint32_t>(change_cycle)
                      : static_cast<uint32_t>(retire_cycle - slack));
      std::snprintf(nm, sizeof nm, "14.%s: both instructions retired", cs.name);
      check(retires == 2, nm, 2, static_cast<uint32_t>(retires));
      std::snprintf(nm, sizeof nm, "14.%s: the answer is not the sentinel", cs.name);
      check(b.read_reg(20) != kSentinel || retire_cycle < 0, nm, 1,
            b.read_reg(20) != kSentinel ? 1 : 0);
    }
  }

  return zhao::report_and_exit("field_seq_directed");
}
