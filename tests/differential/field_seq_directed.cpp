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
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_seq.h"

#include "zfield/zfield.hpp"
#include "zhao_sim.hpp"

namespace {

using zhao::check;

constexpr int32_t kOne = 1 << 16;

/** A program the test builds by hand, in the shape both oracles accept. */
struct Prog {
  std::vector<zfield::Instr> ins;
  std::vector<uint8_t> in_regs;
  std::vector<uint8_t> out_regs;

  void op(uint8_t o, uint8_t dst, uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, uint32_t imm = 0) {
    zfield::Instr i{};
    i.op = o;
    i.dst = dst;
    i.a = a;
    i.b = b;
    i.c = c;
    i.imm = imm;
    ins.push_back(i);
  }
  void end() { op(zfield::OP_END, 0); }
};

std::vector<int32_t> interp(const Prog& p, const std::vector<int32_t>& in, bool* sat,
                            bool* rcp0 = nullptr) {
  zfield::Decoded d;
  d.profile = 0;
  d.instrs = p.ins;
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
    dut.eval();
  }

  void step() {
    present();
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
    dut.rf_raddr_i = r;
    dut.eval();
    return static_cast<int32_t>(dut.rf_rdata_o);
  }

  /** Zero, load, run, read back -- the reference's own order. */
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
    while (!dut.done_o && n < 20000) {
      step();
      if (dut.instr_retired_o) ++ret;
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
  // Two cases, because they age differently.
  //
  // This check previously used OP_RCP, and wiring RCP into the dispatch broke
  // it — correctly, but it meant the refusal law was pinned to whichever op
  // happened to be unimplemented that week. So the SECOND case below uses an
  // opcode that is not in the enum at all and never will be, which is the
  // stable statement of the law. The first case keeps a real-but-unwired op,
  // because "the walk refuses an op it has not been taught" is the thing that
  // actually matters and it should be stated about a real opcode while any
  // remain.
  {
    // A real op that is built and verified as a block but not yet dispatched.
    // Wiring ROT3 will break this line, and the fix is to move it to whatever
    // is still unwired — or to delete it once nothing is.
    Prog p;
    p.in_regs = {0};
    p.op(zfield::OP_MOV, 3, 0);
    p.op(zfield::OP_ROT3, 4, 3);
    p.end();
    p.out_regs = {3};
    uint8_t status = 0;
    int retired = 0;
    b.run(p, {kOne}, &status, nullptr, &retired);
    check(status == 1, "7.a real op outside the dispatch is REFUSED", 1, status);
    check(retired == 1, "7.and the walk stopped there", 1, static_cast<uint32_t>(retired));

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
                            zfield::OP_SIN, zfield::OP_COS};
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
        p.op(op, dst, a, unary ? 0 : bb);
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

  return zhao::report_and_exit("field_seq_directed");
}
