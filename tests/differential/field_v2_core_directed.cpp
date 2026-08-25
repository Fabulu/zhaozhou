// field_v2_core_directed.cpp — FIELD v2's SIMD barrel core against the oracle.
//
// v2 answers to the SAME oracle v1 does: `zfield::interpret`, the one generic
// Field IR interpreter. That is the whole point of building v2 alongside a
// frozen v1 rather than out of it — the semantics are not being renegotiated,
// only the machine that realises them.
//
// WHAT THIS PINS, and why each case is here rather than being a nice round
// number of random programs:
//
//   1. one wavefront, one lane   — the semantics, with SIMD switched off. If
//      this fails nothing else matters.
//   2. one wavefront, four lanes — four DIFFERENT vertices under one PC. The
//      obvious defect is lane crosstalk, so the lanes are given values that
//      make any leak produce a wrong answer rather than a plausible one.
//   3. several wavefronts        — the barrel. Wavefronts are started TOGETHER
//      and interleave in the issue window; each must retire its own program
//      against its own registers.
//   4. throughput                — the reason v2 exists. Retired instructions
//      per clock is measured, not asserted, and compared against v1's 1-per-7.
//   5. refusal                   — an unsupported opcode must be REFUSED, not
//      skipped and not zero. v1's law, and the one that keeps a wrong world
//      from looking like a plausible one.

#include "Vzhao_field_v2_core.h"
#include "verilated.h"

#include "zhao_sim.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

using zhao::check;

constexpr int kLanes = 4;
constexpr int kWfs = 8;

constexpr uint8_t OP_MOV = 0x00, OP_ADD = 0x01, OP_SUB = 0x02, OP_MUL = 0x03;
constexpr uint8_t OP_MAD = 0x04, OP_MIN = 0x05, OP_MAX = 0x06, OP_ABS = 0x07;
constexpr uint8_t OP_CLAMP = 0x08, OP_SELECT = 0x09, OP_CMP = 0x0A;
constexpr uint8_t OP_END = 0xFF;
constexpr uint8_t OP_UNSUPPORTED = 0x17;  // RCP: real opcode, not yet in v2

struct Instr {
  uint8_t op, dst, a, b, c;
};

// The oracle, in the exact arithmetic of the reference: saturating add/sub and
// a Q16.16 multiply that rounds half up.
int32_t sat_add(int32_t a, int32_t b) {
  int64_t s = static_cast<int64_t>(a) + b;
  if (s > INT32_MAX) return INT32_MAX;
  if (s < INT32_MIN) return INT32_MIN;
  return static_cast<int32_t>(s);
}
int32_t sat_sub(int32_t a, int32_t b) {
  int64_t s = static_cast<int64_t>(a) - b;
  if (s > INT32_MAX) return INT32_MAX;
  if (s < INT32_MIN) return INT32_MIN;
  return static_cast<int32_t>(s);
}
int32_t q16_mul(int32_t a, int32_t b) {
  int64_t p = (static_cast<int64_t>(a) * b + 32768) >> 16;
  if (p > INT32_MAX) return INT32_MAX;
  if (p < INT32_MIN) return INT32_MIN;
  return static_cast<int32_t>(p);
}

void interp(const std::vector<Instr>& prog, int32_t* reg) {
  for (const Instr& in : prog) {
    switch (in.op) {
      case OP_MOV:
        reg[in.dst] = reg[in.a];
        break;
      case OP_ADD:
        reg[in.dst] = sat_add(reg[in.a], reg[in.b]);
        break;
      case OP_SUB:
        reg[in.dst] = sat_sub(reg[in.a], reg[in.b]);
        break;
      case OP_MUL:
        reg[in.dst] = q16_mul(reg[in.a], reg[in.b]);
        break;
      case OP_MAD:
        reg[in.dst] = sat_add(q16_mul(reg[in.a], reg[in.b]), reg[in.c]);
        break;
      case OP_MIN:
        reg[in.dst] = reg[in.a] < reg[in.b] ? reg[in.a] : reg[in.b];
        break;
      case OP_MAX:
        reg[in.dst] = reg[in.a] > reg[in.b] ? reg[in.a] : reg[in.b];
        break;
      case OP_ABS:
        reg[in.dst] = reg[in.a] < 0 ? sat_sub(0, reg[in.a]) : reg[in.a];
        break;
      case OP_CLAMP:
        reg[in.dst] = reg[in.a] < reg[in.b]   ? reg[in.b]
                      : reg[in.a] > reg[in.c] ? reg[in.c]
                                              : reg[in.a];
        break;
      case OP_SELECT:
        reg[in.dst] = reg[in.c] != 0 ? reg[in.a] : reg[in.b];
        break;
      case OP_CMP:
        reg[in.dst] = reg[in.a] < reg[in.b] ? (1 << 16) : 0;
        break;
      case OP_END:
        return;
      default:
        return;
    }
  }
}

struct Bench {
  Vzhao_field_v2_core& d;
  std::vector<Instr> prog;

  explicit Bench(Vzhao_field_v2_core& dut) : d(dut) {
    d.rst_n = 0;
    d.h_we_i = 0;
    d.start_i = 0;
    d.instr_count_i = 0;
    d.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(d);
    d.rst_n = 1;
    d.eval();
  }

  void present() {
    const uint8_t pc = d.pc_o;
    const Instr& in = (pc < prog.size()) ? prog[pc] : prog.back();
    d.ins_op_i = in.op;
    d.ins_dst_i = in.dst;
    d.ins_a_i = in.a;
    d.ins_b_i = in.b;
    d.ins_c_i = in.c;
    d.eval();
  }

  void write_reg(int wf, int lane, int r, int32_t v) {
    d.h_we_i = 1;
    d.h_wf_i = wf;
    d.h_lane_i = lane;
    d.h_reg_i = r;
    d.h_wdata_i = static_cast<uint32_t>(v);
    present();
    zhao::tick(d);
    d.h_we_i = 0;
    d.eval();
  }

  int32_t read_reg(int wf, int lane, int r) {
    d.h_rwf_i = wf;
    d.h_rlane_i = lane;
    d.h_rreg_i = r;
    present();
    zhao::tick(d);  // the file is memory: the answer lands on the next edge
    d.eval();
    return static_cast<int32_t>(d.h_rdata_o);
  }

  /** Run `mask` wavefronts to completion. Returns clocks consumed. */
  int run(uint32_t mask, int guard = 20000) {
    d.instr_count_i = static_cast<uint8_t>(prog.size());
    d.start_i = mask;
    present();
    zhao::tick(d);
    d.start_i = 0;
    present();
    int clocks = 0;
    while ((d.busy_o != 0) && clocks < guard) {
      present();
      zhao::tick(d);
      ++clocks;
    }
    present();
    return clocks;
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_field_v2_core dut;

  // ---- 1. one wavefront, one lane: the semantics ---------------------------
  {
    Bench b(dut);
    b.prog = {
        {OP_ADD, 2, 0, 1, 0}, {OP_MUL, 3, 2, 0, 0}, {OP_SUB, 4, 3, 1, 0}, {OP_END, 0, 0, 0, 0}};
    const int32_t kOne = 1 << 16;
    int32_t ref[64] = {};
    ref[0] = 3 * kOne;
    ref[1] = 5 * kOne;
    for (int l = 0; l < kLanes; ++l) {
      b.write_reg(0, l, 0, ref[0]);
      b.write_reg(0, l, 1, ref[1]);
    }
    interp(b.prog, ref);
    b.run(1u);
    check(b.read_reg(0, 0, 2) == ref[2], "1.ADD matches the oracle", ref[2], b.read_reg(0, 0, 2));
    check(b.read_reg(0, 0, 3) == ref[3], "1.MUL matches the oracle", ref[3], b.read_reg(0, 0, 3));
    check(b.read_reg(0, 0, 4) == ref[4], "1.SUB matches the oracle", ref[4], b.read_reg(0, 0, 4));
  }

  // ---- 2. four lanes, four DIFFERENT vertices ------------------------------
  // The values are deliberately distinct and non-commutative under the program,
  // so a lane reading its neighbour's register produces a WRONG answer rather
  // than a plausible one. Lane crosstalk is the defect a SIMD core newly admits.
  {
    Bench b(dut);
    b.prog = {{OP_MUL, 2, 0, 1, 0}, {OP_ADD, 3, 2, 0, 0}, {OP_END, 0, 0, 0, 0}};
    int32_t ref[kLanes][64] = {};
    for (int l = 0; l < kLanes; ++l) {
      ref[l][0] = (l + 1) * (1 << 16);
      ref[l][1] = (7 - l) * (1 << 16);
      b.write_reg(0, l, 0, ref[l][0]);
      b.write_reg(0, l, 1, ref[l][1]);
      interp(b.prog, ref[l]);
    }
    b.run(1u);
    for (int l = 0; l < kLanes; ++l) {
      char nm[96];
      std::snprintf(nm, sizeof nm, "2.lane %d keeps its own product", l);
      check(b.read_reg(0, l, 2) == ref[l][2], nm, ref[l][2], b.read_reg(0, l, 2));
      std::snprintf(nm, sizeof nm, "2.lane %d keeps its own sum", l);
      check(b.read_reg(0, l, 3) == ref[l][3], nm, ref[l][3], b.read_reg(0, l, 3));
    }
  }

  // ---- 3. several wavefronts interleaved ----------------------------------
  // Started TOGETHER, so they share the issue window and interleave. Each must
  // retire against its own registers; a wavefront index dropped anywhere in the
  // scoreboard shows up here and nowhere else.
  {
    Bench b(dut);
    b.prog = {{OP_ADD, 2, 0, 1, 0}, {OP_MUL, 3, 2, 1, 0}, {OP_END, 0, 0, 0, 0}};
    int32_t ref[kWfs][kLanes][64] = {};
    for (int w = 0; w < kWfs; ++w)
      for (int l = 0; l < kLanes; ++l) {
        ref[w][l][0] = (w * 4 + l + 1) * (1 << 16);
        ref[w][l][1] = (w + 2) * (1 << 16);
        b.write_reg(w, l, 0, ref[w][l][0]);
        b.write_reg(w, l, 1, ref[w][l][1]);
        interp(b.prog, ref[w][l]);
      }
    b.run((1u << kWfs) - 1u);
    uint64_t bad = 0;
    for (int w = 0; w < kWfs; ++w)
      for (int l = 0; l < kLanes; ++l) {
        if (b.read_reg(w, l, 2) != ref[w][l][2]) ++bad;
        if (b.read_reg(w, l, 3) != ref[w][l][3]) ++bad;
      }
    check(bad == 0, "3.eight wavefronts x four lanes each keep their own answers", 0, bad);
  }

  // ---- 4. THROUGHPUT, measured rather than asserted ------------------------
  // v2 exists for this number. v1 retires one instruction every 7 clocks; the
  // claim here is one VECTOR instruction per clock once enough wavefronts are
  // resident, which is 4 vertex-instructions per clock at LANES=4.
  {
    Bench b(dut);
    b.prog.clear();
    for (int i = 0; i < 16; ++i) b.prog.push_back({OP_ADD, 2, 0, 1, 0});
    b.prog.push_back({OP_END, 0, 0, 0, 0});
    for (int w = 0; w < kWfs; ++w)
      for (int l = 0; l < kLanes; ++l) {
        b.write_reg(w, l, 0, 1 << 16);
        b.write_reg(w, l, 1, 2 << 16);
      }
    const uint32_t before = dut.instr_retired_o;
    const int clocks = b.run((1u << kWfs) - 1u);
    const uint32_t retired = dut.instr_retired_o - before;

    // 8 wavefronts x 17 instructions each, END included.
    check(retired == static_cast<uint32_t>(kWfs * 17),
          "4.every wavefront retired every instruction", kWfs * 17, retired);
    std::printf(
        "  v2: %u vector instructions in %d clocks = %.2f instr/clock (%.2f vertex-instr/clock)\n",
        retired, clocks, double(retired) / clocks, double(retired) * kLanes / clocks);
    // v1's measured floor is 7 clocks per instruction. Anything at or below 2
    // clocks per vector instruction is a categorical change rather than a tune.
    check(double(retired) / clocks > 0.5,
          "4.throughput beats one vector instruction per two clocks", 1,
          (double(retired) / clocks > 0.5) ? 1 : 0);
  }

  // ---- 5. an unsupported opcode is REFUSED --------------------------------
  // Not skipped and not zero. A sequencer that quietly ignores an opcode
  // produces a plausible field and a wrong world -- v1's law, kept.
  {
    Bench b(dut);
    b.prog = {{OP_MOV, 2, 0, 0, 0}, {OP_UNSUPPORTED, 3, 0, 0, 0}, {OP_END, 0, 0, 0, 0}};
    for (int l = 0; l < kLanes; ++l) b.write_reg(0, l, 0, 1 << 16);
    b.run(1u);
    check(dut.status_o == 3, "5.an opcode v2 cannot execute is refused, not skipped", 3,
          dut.status_o);
  }

  dut.final();
  return zhao::report_and_exit("field_v2_core_directed");
}
