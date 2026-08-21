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

std::vector<int32_t> interp(const Prog& p, const std::vector<int32_t>& in, bool* sat) {
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
  const std::vector<int32_t> want = interp(p, in, &want_sat);
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
  const bool got_sat = b.dut.sat_add_o || b.dut.sat_mul_o || b.dut.sat_rescale_o;
  check(got_sat == want_sat, (t + ": Status.sat").c_str(), want_sat ? 1 : 0, got_sat ? 1 : 0);
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
  {
    Prog p;
    p.in_regs = {0};
    p.op(zfield::OP_MOV, 3, 0);
    p.op(zfield::OP_RCP, 4, 3);  // not in this increment's dispatch
    p.end();
    p.out_regs = {3};
    uint8_t status = 0;
    int retired = 0;
    b.run(p, {kOne}, &status, nullptr, &retired);
    check(status == 1, "7.an op outside the core is REFUSED with a status", 1, status);
    check(retired == 1, "7.and the walk stopped there", 1, static_cast<uint32_t>(retired));
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
    const uint8_t pool[] = {zfield::OP_MOV, zfield::OP_ADD, zfield::OP_SUB, zfield::OP_MUL,
                            zfield::OP_MIN, zfield::OP_MAX, zfield::OP_ABS};
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
        const bool unary = (op == zfield::OP_MOV || op == zfield::OP_ABS);
        p.op(op, dst, a, unary ? 0 : bb);
        defined_hi = dst;
      }
      p.end();
      p.out_regs = {static_cast<uint8_t>(defined_hi)};
      std::vector<int32_t> in;
      for (int i = 0; i < n_in; ++i) in.push_back(static_cast<int32_t>(rng.next()));
      char nm[64];
      std::snprintf(nm, sizeof nm, "10.random[%d]", it);
      diff(b, p, in, nm);
    }
    std::printf("random: %d programs\n", random_iters);
  }

  return zhao::report_and_exit("field_seq_directed");
}
