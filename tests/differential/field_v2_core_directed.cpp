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
#include "zfield/zfield.hpp"
#include "zref/zref_fixp.hpp"

#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

namespace {

using zhao::check;

constexpr int kLanes = 4;
constexpr int kWfs = 8;

constexpr uint8_t OP_MOV = 0x00, OP_ADD = 0x01, OP_SUB = 0x02, OP_MUL = 0x03;
constexpr uint8_t OP_MAD = 0x04, OP_MIN = 0x05, OP_MAX = 0x06, OP_ABS = 0x07;
constexpr uint8_t OP_CLAMP = 0x08, OP_SELECT = 0x09, OP_CMP = 0x0A;
constexpr uint8_t OP_END = 0xFF;
constexpr uint8_t OP_CURVE = 0x1A;        // dispatched to the serialiser
constexpr uint8_t OP_SPLINE = 0x1B;       // same unit, mode 2
constexpr uint8_t OP_DCURVE = 0x1D;       // same unit, mode 1
constexpr uint8_t OP_LEN2 = 0x12;         // the length unit, mode 0
constexpr uint8_t OP_LEN3 = 0x13;         // the length unit, mode 1
constexpr uint8_t OP_DIST2 = 0x14;        // the length unit, mode 2
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
    d.tbl_n_i = 0;
    d.tbl_x_i = 0;
    d.tbl_y_i = 0;
    d.tbl_dy_i = 0;
    d.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(d);
    d.rst_n = 1;
    d.eval();
  }

  // The curve table, as a REGISTERED read: the index presented this cycle is
  // answered on the NEXT one, which is what an M10K does.
  //
  // I first drove this combinationally -- answering the current index in the
  // same evaluation -- and every lane came back zero, because the unit was fed
  // data one cycle early on every lookup. v1's own curve bench states the
  // protocol in a comment and I had not read it.
  std::vector<int32_t> tx, ty, tdy;

  void tick_tbl() {
    const uint32_t idx = d.tbl_idx_o;
    zhao::tick(d);
    if (!tx.empty()) {
      const bool in = idx < tx.size();
      // Out-of-range answers are HOSTILE: a missing bound check should walk off
      // the end loudly rather than quietly agreeing with the oracle.
      d.tbl_x_i = static_cast<uint32_t>(in ? tx[idx] : INT32_MIN);
      d.tbl_y_i = static_cast<uint32_t>(in ? ty[idx] : 0x5A5A5A5A);
      d.tbl_dy_i = static_cast<uint32_t>(in ? tdy[idx] : 0x5A5A5A5A);
      d.eval();
    }
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
      tick_tbl();
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

  // ---- 6. CURVE: a LONG op, through the tagged serialiser -----------------
  // The first long operation v2 can run. It is not executed in the core: the
  // core dispatches a vector request to zhao_field_v2_lanemux, which serialises
  // four lanes through v1's UNMODIFIED zhao_field_curve and carries the
  // {wavefront, destination} tag back.
  //
  // The oracle is zfield::interpret on a two-instruction program -- the same
  // oracle v1's own curve differential uses. v2 changes how work reaches the
  // unit, not what it computes, so agreement here is the claim worth making.
  //
  // FOUR DISTINCT LANE VALUES, because the defect this path newly admits is a
  // lane receiving another lane's answer, and equal inputs would hide it.
  {
    Bench b(dut);
    b.prog = {{OP_CURVE, 2, 0, 0, 0}, {OP_END, 0, 0, 0, 0}};
    const std::vector<int32_t> tx = {0, 1 << 16, 2 << 16, 3 << 16};
    const std::vector<int32_t> ty = {0, 2 << 16, 1 << 16, 4 << 16};
    const std::vector<int32_t> tdy(4, 0);

    auto oracle = [&](uint8_t op, int32_t a) -> int32_t {
      zfield::Decoded prog;
      prog.profile = 0;
      prog.tables.push_back(zfield::Table{0, tx, ty, tdy});
      zfield::Instr ins{};
      ins.op = op;
      ins.dst = 1;
      ins.a = 0;
      ins.b = 0;
      ins.c = 0;
      ins.imm = 0;
      prog.instrs.push_back(ins);
      zfield::Instr end{};
      end.op = zfield::OP_END;
      prog.instrs.push_back(end);
      zfield::IoLane il{};
      il.name = "a";
      il.type = 0;
      il.reg = 0;
      prog.in_lanes.push_back(il);
      zfield::IoLane ol{};
      ol.name = "y";
      ol.type = 0;
      ol.reg = 1;
      prog.out_lanes.push_back(ol);
      int32_t in[1] = {a};
      int32_t out[1] = {0};
      zfield::interpret(prog, in, 1, out, 1);
      return out[0];
    };

    int32_t a[kLanes];
    for (int l = 0; l < kLanes; ++l) {
      a[l] = (l + 1) * (1 << 15);
      b.write_reg(0, l, 0, a[l]);
    }
    b.tx = tx;
    b.ty = ty;
    b.tdy = tdy;
    dut.tbl_n_i = static_cast<uint8_t>(tx.size());
    b.run(1u);

    uint64_t bad = 0;
    for (int l = 0; l < kLanes; ++l) {
      const int32_t want = oracle(zfield::OP_CURVE, a[l]);
      const int32_t got = b.read_reg(0, l, 2);
      if (got != want) {
        ++bad;
        std::printf("  6.lane %d: want %d got %d\n", l, want, got);
      }
    }
    check(bad == 0, "6.CURVE through the serialiser matches zfield::interpret, per lane", 0, bad);
    check(dut.status_o == 0, "6.and the run reports OK, not refused", 0, dut.status_o);
  }

  // ---- 7. DCURVE and SPLINE: the same unit, different modes ---------------
  // Both were wired with CURVE -- one unit, a mode select -- so they need
  // EVIDENCE rather than RTL. Worth testing separately anyway: the mode is
  // carried through the serialiser as part of the request, so a mode dropped or
  // mistranslated between the core's opcode and the unit's 2-bit input would
  // show up here and nowhere else. Section 6 alone would pass with the mode
  // hard-wired to zero.
  {
    struct ModeCase {
      uint8_t op;
      uint8_t z_op;
      const char* name;
    };
    const ModeCase cases[] = {
        {OP_DCURVE, zfield::OP_DCURVE, "DCURVE"},
        {OP_SPLINE, zfield::OP_SPLINE, "SPLINE"},
    };
    const std::vector<int32_t> tx = {0, 1 << 16, 2 << 16, 3 << 16};
    const std::vector<int32_t> ty = {0, 2 << 16, 1 << 16, 4 << 16};
    const std::vector<int32_t> tdy = {0, 1 << 15, -(1 << 15), 0};

    for (const ModeCase& mc : cases) {
      Bench b(dut);
      b.prog = {{mc.op, 2, 0, 0, 0}, {OP_END, 0, 0, 0, 0}};

      auto oracle = [&](int32_t a) -> int32_t {
        zfield::Decoded prog;
        prog.profile = 0;
        prog.tables.push_back(zfield::Table{0, tx, ty, tdy});
        zfield::Instr ins{};
        ins.op = mc.z_op;
        ins.dst = 1;
        ins.a = 0;
        ins.b = 0;
        ins.c = 0;
        ins.imm = 0;
        prog.instrs.push_back(ins);
        zfield::Instr end{};
        end.op = zfield::OP_END;
        prog.instrs.push_back(end);
        zfield::IoLane il{};
        il.name = "a";
        il.type = 0;
        il.reg = 0;
        prog.in_lanes.push_back(il);
        zfield::IoLane ol{};
        ol.name = "y";
        ol.type = 0;
        ol.reg = 1;
        prog.out_lanes.push_back(ol);
        int32_t in[1] = {a};
        int32_t out[1] = {0};
        zfield::interpret(prog, in, 1, out, 1);
        return out[0];
      };

      int32_t a[kLanes];
      for (int l = 0; l < kLanes; ++l) {
        a[l] = (l + 1) * (1 << 15);
        b.write_reg(0, l, 0, a[l]);
      }
      b.tx = tx;
      b.ty = ty;
      b.tdy = tdy;
      dut.tbl_n_i = static_cast<uint8_t>(tx.size());
      b.run(1u);

      uint64_t bad = 0;
      for (int l = 0; l < kLanes; ++l)
        if (b.read_reg(0, l, 2) != oracle(a[l])) ++bad;
      char nm[96];
      std::snprintf(nm, sizeof nm, "7.%s matches zfield::interpret on every lane", mc.name);
      check(bad == 0, nm, 0, bad);
    }
  }

  // ---- 8. EVERY wavefront running long ops, in TWO MODES at once ----------
  // Sections 6 and 7 each start ONE wavefront, so at most one long operation is
  // ever in the machine. That is not the workload: eight wavefronts run the same
  // Earth program, they drift apart in pc, and the serialiser sees requests from
  // several of them interleaved.
  //
  // The mutation sweep is what said so. M93 (a long op counted at dispatch AND
  // at reply), M94 (the unit given the LIVE mode rather than the captured one)
  // and M95 (a request retired without the serialiser accepting it) all SURVIVED
  // sections 6 and 7, and all three are unreachable with a single long op in
  // flight. This section is the smallest program that reaches them: CURVE then
  // SPLINE, so two wavefronts at different pcs demand DIFFERENT MODES of the
  // same unit in the same window.
  {
    Bench b(dut);
    b.prog = {{OP_CURVE, 2, 0, 0, 0}, {OP_SPLINE, 3, 0, 0, 0}, {OP_END, 0, 0, 0, 0}};
    const std::vector<int32_t> tx = {0, 1 << 16, 2 << 16, 3 << 16};
    const std::vector<int32_t> ty = {0, 2 << 16, 1 << 16, 4 << 16};
    const std::vector<int32_t> tdy = {0, 1 << 15, -(1 << 15), 0};

    auto oracle = [&](uint8_t z_op, int32_t a) -> int32_t {
      zfield::Decoded prog;
      prog.profile = 0;
      prog.tables.push_back(zfield::Table{0, tx, ty, tdy});
      zfield::Instr ins{};
      ins.op = z_op;
      ins.dst = 1;
      ins.a = 0;
      ins.b = 0;
      ins.c = 0;
      ins.imm = 0;
      prog.instrs.push_back(ins);
      zfield::Instr end{};
      end.op = zfield::OP_END;
      prog.instrs.push_back(end);
      zfield::IoLane il{};
      il.name = "a";
      il.type = 0;
      il.reg = 0;
      prog.in_lanes.push_back(il);
      zfield::IoLane ol{};
      ol.name = "y";
      ol.type = 0;
      ol.reg = 1;
      prog.out_lanes.push_back(ol);
      int32_t in[1] = {a};
      int32_t out[1] = {0};
      zfield::interpret(prog, in, 1, out, 1);
      return out[0];
    };

    // Distinct per wavefront AND per lane: a reply delivered to the wrong
    // wavefront, or the wrong lane of the right one, must not land on a value
    // that happens to be correct anyway.
    int32_t a[kWfs][kLanes];
    for (int w = 0; w < kWfs; ++w)
      for (int l = 0; l < kLanes; ++l) {
        a[w][l] = (w * kLanes + l + 1) * 3719;
        b.write_reg(w, l, 0, a[w][l]);
      }

    b.tx = tx;
    b.ty = ty;
    b.tdy = tdy;
    dut.tbl_n_i = static_cast<uint8_t>(tx.size());
    const uint32_t before = dut.instr_retired_o;
    const int clocks = b.run((1u << kWfs) - 1u);

    check(clocks < 20000, "8.all wavefronts finished (no lost long-op request)", 1,
          clocks < 20000 ? 1 : 0);

    uint64_t bad_c = 0, bad_s = 0;
    for (int w = 0; w < kWfs; ++w)
      for (int l = 0; l < kLanes; ++l) {
        if (b.read_reg(w, l, 2) != oracle(zfield::OP_CURVE, a[w][l])) ++bad_c;
        if (b.read_reg(w, l, 3) != oracle(zfield::OP_SPLINE, a[w][l])) ++bad_s;
      }
    check(bad_c == 0, "8.CURVE result correct for every wavefront and lane", 0, bad_c);
    check(bad_s == 0, "8.SPLINE result correct for every wavefront and lane", 0, bad_s);

    // A long op retires ONCE, at its reply. Counting it at dispatch as well is
    // invisible to any value check -- the answers stay right and the machine
    // merely claims to have done more work than it did.
    const uint32_t retired = dut.instr_retired_o - before;
    check(retired == static_cast<uint32_t>(kWfs * 3),
          "8.each wavefront retired exactly its three instructions", kWfs * 3, retired);
    std::printf("  v2 long-op mix: %u instructions in %d clocks\n", retired, clocks);
  }

  // ---- 9. THE LENGTH FAMILY: LEN2, LEN3, DIST2 ----------------------------
  // The second long-op unit, and the first that needs MORE OPERANDS THAN THE
  // REGISTER FILE HANDS OUT. LEN3 reads a, a+1, a+2; DIST2 also reads b, b+1.
  // Three read ports supply a/b/c and `a+1` is reachable from none of them.
  //
  // The core spends ONE CLOCK instead of two more register-file ports: with the
  // length held in stage 1 it redirects those same three ports at
  // {a+1, a+2, b+1} and dispatches a cycle later. The alternative was ~4 more
  // M10K permanently (the banked file measured 12 at three ports) for a
  // minority of the opcode histogram.
  //
  // Oracle: zfield::interpret, the same one v1's length differential uses, on
  // the same unit -- v2 changes how work reaches zhao_field_len, not what it
  // computes.
  {
    struct LenCase {
      uint8_t op;
      uint8_t z_op;
      int comps;  // how many registers the op consumes from base a
      bool two_bases;
      const char* name;
    };
    const LenCase cases[] = {
        {OP_LEN2, zfield::OP_LEN2, 2, false, "LEN2"},
        {OP_LEN3, zfield::OP_LEN3, 3, false, "LEN3"},
        {OP_DIST2, zfield::OP_DIST2, 2, true, "DIST2"},
    };

    for (const LenCase& lc : cases) {
      Bench b(dut);
      // Base a = r0, base b = r8. Deliberately NOT adjacent: a run that
      // confused the two bases would still land inside the file, and equal or
      // neighbouring bases would let a wrong address read a right value.
      b.prog = {{lc.op, 16, 0, 8, 0}, {OP_END, 0, 0, 0, 0}};

      auto oracle = [&](const int32_t* av, const int32_t* bv) -> int32_t {
        zfield::Decoded prog;
        prog.profile = 0;
        zfield::Instr ins{};
        ins.op = lc.z_op;
        ins.dst = 16;
        ins.a = 0;
        ins.b = 8;
        ins.c = 0;
        ins.imm = 0;
        prog.instrs.push_back(ins);
        zfield::Instr end{};
        end.op = zfield::OP_END;
        prog.instrs.push_back(end);
        int32_t in[16] = {0};
        // The interpreter's registers are the machine's registers: place the
        // operands where the instruction says they are, not where a helper
        // would like them.
        for (int k = 0; k < lc.comps + 1; ++k) in[k] = av[k];
        int32_t out[1] = {0};
        for (int k = 0; k < 16; ++k) {
          zfield::IoLane il{};
          il.name = "i";
          il.type = 0;
          il.reg = static_cast<uint8_t>(k);
          prog.in_lanes.push_back(il);
        }
        if (lc.two_bases) {
          in[8] = bv[0];
          in[9] = bv[1];
        }
        zfield::IoLane ol{};
        ol.name = "y";
        ol.type = 0;
        ol.reg = 16;
        prog.out_lanes.push_back(ol);
        zfield::interpret(prog, in, 16, out, 1);
        return out[0];
      };

      int32_t av[kWfs][kLanes][3];
      int32_t bv[kWfs][kLanes][2];
      for (int w = 0; w < kWfs; ++w)
        for (int l = 0; l < kLanes; ++l) {
          for (int k = 0; k < 3; ++k) {
            av[w][l][k] = (w * kLanes + l + 1) * (k + 1) * 977;
            b.write_reg(w, l, k, av[w][l][k]);
          }
          for (int k = 0; k < 2; ++k) {
            bv[w][l][k] = -((w * kLanes + l + 1) * (k + 3) * 613);
            b.write_reg(w, l, 8 + k, bv[w][l][k]);
          }
        }

      const uint32_t before = dut.instr_retired_o;
      const int clocks = b.run((1u << kWfs) - 1u);

      uint64_t bad = 0;
      // DISTINCT, NON-ZERO answers. Every check in this section is "DUT equals
      // oracle", and that passes just as happily when both are zero -- which is
      // what a mis-mapped io lane or an unrun program would produce. So the
      // oracle is required to be non-degenerate before its agreement counts.
      std::set<int32_t> distinct;
      uint64_t zero_oracle = 0;
      for (int w = 0; w < kWfs; ++w)
        for (int l = 0; l < kLanes; ++l) {
          const int32_t want = oracle(av[w][l], bv[w][l]);
          if (want == 0) ++zero_oracle;
          distinct.insert(want);
          if (b.read_reg(w, l, 16) != want) ++bad;
        }
      check(zero_oracle == 0, "9.oracle is non-degenerate (no zero answers)", 0, zero_oracle);
      check(distinct.size() == static_cast<size_t>(kWfs * kLanes),
            "9.every wavefront/lane has a DISTINCT answer", kWfs * kLanes, distinct.size());

      char nm[96];
      std::snprintf(nm, sizeof nm, "9.%s matches zfield::interpret on every wavefront and lane",
                    lc.name);
      check(bad == 0, nm, 0, bad);

      const uint32_t retired = dut.instr_retired_o - before;
      std::snprintf(nm, sizeof nm, "9.%s retired exactly two instructions per wavefront", lc.name);
      check(retired == static_cast<uint32_t>(kWfs * 2), nm, kWfs * 2, retired);
      std::printf("  v2 %-6s %u instructions in %d clocks\n", lc.name, retired, clocks);
    }
  }

  // ---- 10. A SHORT OP ISSUING INTO THE STEAL CYCLE ------------------------
  // The steal reuses the three register read ports. An instruction that issues
  // into that cycle has its OWN operand reads replaced by the length's second
  // pass, and then computes on another instruction's values -- a wrong answer,
  // not a hang, which makes it worse than the failure the long-op interlock
  // prevents.
  //
  // Section 9 cannot reach it: its program is one length and an END, so the
  // only thing that could issue into a steal is another length. This program
  // surrounds the length with ADDs, on registers the length never touches, so a
  // stolen read shows up as a wrong SUM rather than a wrong length.
  //
  // The ADDs are checked against plain C++ rather than the interpreter: what
  // must be proven here is that they were NOT disturbed, and the simplest
  // statement of an undisturbed add is the add itself.
  {
    Bench b(dut);
    b.prog = {
        {OP_ADD, 20, 12, 13, 0},  // short, before the length
        {OP_LEN3, 16, 0, 8, 0},   // long, and steals a read cycle
        {OP_ADD, 21, 13, 14, 0},  // short, in the window the steal opens
        {OP_ADD, 22, 12, 14, 0},  // short, right behind it
        {OP_END, 0, 0, 0, 0},
    };

    int32_t r[kWfs][kLanes][15];
    for (int w = 0; w < kWfs; ++w)
      for (int l = 0; l < kLanes; ++l)
        for (int k = 0; k < 15; ++k) {
          r[w][l][k] = (w * kLanes + l + 1) * (k + 1) * 131 - 7717;
          b.write_reg(w, l, k, r[w][l][k]);
        }

    const int clocks = b.run((1u << kWfs) - 1u);
    check(clocks < 20000, "10.mixed short/long program finished", 1, clocks < 20000 ? 1 : 0);

    uint64_t bad_add = 0;
    for (int w = 0; w < kWfs; ++w)
      for (int l = 0; l < kLanes; ++l) {
        if (b.read_reg(w, l, 20) != r[w][l][12] + r[w][l][13]) ++bad_add;
        if (b.read_reg(w, l, 21) != r[w][l][13] + r[w][l][14]) ++bad_add;
        if (b.read_reg(w, l, 22) != r[w][l][12] + r[w][l][14]) ++bad_add;
      }
    check(bad_add == 0, "10.no short op was disturbed by the steal cycle", 0, bad_add);

    // And the length itself still has to be right, or "undisturbed" could be
    // bought by not stealing at all.
    uint64_t bad_len = 0;
    for (int w = 0; w < kWfs; ++w)
      for (int l = 0; l < kLanes; ++l) {
        zfield::Decoded prog;
        prog.profile = 0;
        zfield::Instr ins{};
        ins.op = zfield::OP_LEN3;
        ins.dst = 16;
        ins.a = 0;
        ins.b = 8;
        ins.c = 0;
        ins.imm = 0;
        prog.instrs.push_back(ins);
        zfield::Instr end{};
        end.op = zfield::OP_END;
        prog.instrs.push_back(end);
        int32_t in[15];
        for (int k = 0; k < 15; ++k) {
          in[k] = r[w][l][k];
          zfield::IoLane il{};
          il.name = "i";
          il.type = 0;
          il.reg = static_cast<uint8_t>(k);
          prog.in_lanes.push_back(il);
        }
        zfield::IoLane ol{};
        ol.name = "y";
        ol.type = 0;
        ol.reg = 16;
        prog.out_lanes.push_back(ol);
        int32_t out[1] = {0};
        zfield::interpret(prog, in, 15, out, 1);
        if (b.read_reg(w, l, 16) != out[0]) ++bad_len;
      }
    check(bad_len == 0, "10.and the length beside them is still correct", 0, bad_len);
    std::printf("  v2 mixed short/long: %d clocks\n", clocks);
  }

  // ---- 12. THE SATURATION LEDGER, which values alone never test -----------
  // Found by the sweep, not by inspection. M106 makes the length's saturation
  // reach the ledger only when a curve also saturated -- and it SURVIVED every
  // section above, because every section above checks values and saturation is
  // not a value.
  //
  // It matters for the same reason the dangling sat_* pins mattered when CURVE
  // landed: this engine's answer is the number AND the account of what it had
  // to clamp to produce it. An engine that drops the account reports clean
  // arithmetic over clamped arithmetic, which is worse than reporting nothing.
  //
  // Expectation from zref::SatLedger + zref::fx_sub, the same construction
  // field_len_directed.cpp uses for this question.
  {
    // ---- 12a. a DIST2 whose difference cannot fit -------------------------
    {
      Bench b(dut);
      b.prog = {{OP_DIST2, 16, 0, 8, 0}, {OP_END, 0, 0, 0, 0}};
      const int32_t a0 = INT32_MAX, a1 = 0, b0 = INT32_MIN, b1 = 0;

      zref::SatLedger L{};
      (void)zref::fx_sub(zref::fx16{a0}, zref::fx16{b0}, &L);
      (void)zref::fx_sub(zref::fx16{a1}, zref::fx16{b1}, &L);
      const bool want_add = L.add != 0;
      check(want_add, "12.the chosen operands really do saturate (oracle)", 1, want_add ? 1 : 0);

      for (int w = 0; w < kWfs; ++w)
        for (int l = 0; l < kLanes; ++l) {
          b.write_reg(w, l, 0, a0);
          b.write_reg(w, l, 1, a1);
          b.write_reg(w, l, 8, b0);
          b.write_reg(w, l, 9, b1);
        }
      b.run((1u << kWfs) - 1u);
      check(dut.sat_add_o == 1, "12.a saturating length reaches SatLedger::add", 1, dut.sat_add_o);
    }

    // ---- 12b. and a length that does NOT saturate -------------------------
    // Without this, a ledger wired stuck-at-one would pass 12a. The lanes are
    // sticky across a run, so this needs its own reset -- which is what a fresh
    // Bench is.
    {
      Bench b(dut);
      b.prog = {{OP_DIST2, 16, 0, 8, 0}, {OP_END, 0, 0, 0, 0}};
      const int32_t a0 = 3 << 16, a1 = 4 << 16, b0 = 0, b1 = 0;

      zref::SatLedger L{};
      (void)zref::fx_sub(zref::fx16{a0}, zref::fx16{b0}, &L);
      (void)zref::fx_sub(zref::fx16{a1}, zref::fx16{b1}, &L);
      check(L.add == 0, "12.the quiet operands really do not saturate (oracle)", 0, L.add);

      for (int w = 0; w < kWfs; ++w)
        for (int l = 0; l < kLanes; ++l) {
          b.write_reg(w, l, 0, a0);
          b.write_reg(w, l, 1, a1);
          b.write_reg(w, l, 8, b0);
          b.write_reg(w, l, 9, b1);
        }
      b.run((1u << kWfs) - 1u);
      check(dut.sat_add_o == 0, "12.a quiet length leaves SatLedger::add alone", 0, dut.sat_add_o);
      // 3-4-5: the answer is still right while nothing clamped.
      uint64_t bad = 0;
      for (int w = 0; w < kWfs; ++w)
        for (int l = 0; l < kLanes; ++l)
          if (b.read_reg(w, l, 16) != (5 << 16)) ++bad;
      check(bad == 0, "12.and the quiet length still answers 5.0", 0, bad);
    }
  }

  dut.final();
  return zhao::report_and_exit("field_v2_core_directed");
}
