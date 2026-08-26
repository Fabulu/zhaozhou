// field_v2_front_directed.cpp — zhao_field_v2_front against zfield::interpret.
//
// The core's differential proves the engine computes. This proves the engine is
// USABLE: a program loaded, a stream of points in, a stream of answers out, one
// per point, IN ORDER.
//
// The oracle is the reference's own driving pattern. zref::terrain walks a patch
// lattice and calls zfield::interpret once per point (terrain.cpp), and
// zfield.hpp states the mapping this block implements: "`in` lanes map to R0..
// in the program's input order; `out` lanes are read from the output map at
// END."
//
// ORDER IS A CORRECTNESS PROPERTY HERE, not a nicety. The reference accumulates
// out[0] into a lattice index derived from its loop counters, so a driver that
// returns the right answers in the wrong order builds the wrong terrain and
// every per-point value check still passes. Section 3 exists for that alone.

#include "Vzhao_field_v2_front.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zfield/zfield.hpp"

#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

namespace {

using zhao::check;

constexpr int kLanes = 4;
constexpr int kWfs = 8;
constexpr int kSlots = kLanes * kWfs;
constexpr int kNin = 12;
constexpr int kNout = 4;

// ---------------------------------------------------------------------------
// TWO ENCODINGS FOR ONE INSTRUCTION SET, and they are NOT the same numbers.
// ---------------------------------------------------------------------------
// zhao_field_v2_core has its own compact ALU encoding:
//
//     MOV 0x00  ADD 0x01  SUB 0x02  MUL 0x03  MAD 0x04 ...  END 0xFF
//
// while the reference (zfield.hpp) uses:
//
//     END 0x00  MOV 0x01  ADD 0x03  MUL 0x05 ...
//
// They collide in the worst possible way: every reference opcode is a VALID but
// DIFFERENT core opcode. Sending reference numbers to the core does not fault --
// it silently computes something else. The first version of this file did
// exactly that, and the symptoms were an ADD returning a product, a MUL
// returning min(a,b), and END writing a register.
//
// So the DUT program is written in CORE opcodes, the oracle program in
// REFERENCE opcodes, and `to_ref()` is the single place the mapping lives.
constexpr uint8_t OP_MOV = 0x00;
constexpr uint8_t OP_ADD = 0x01;
constexpr uint8_t OP_MUL = 0x03;
constexpr uint8_t OP_END = 0xFF;

/** Core opcode -> reference opcode. The only translation in this file. */
uint8_t to_ref(uint8_t core_op) {
  switch (core_op) {
    case OP_MOV:
      return 0x01;  // zfield::OP_MOV
    case OP_ADD:
      return 0x03;  // zfield::OP_ADD
    case 0x02:
      return 0x04;  // SUB
    case OP_MUL:
      return 0x05;  // zfield::OP_MUL
    case 0x04:
      return 0x06;  // MAD
    case OP_END:
      return 0x00;  // zfield::OP_END
    default:
      return 0xFE;  // deliberately invalid: a new op must be added here
  }
}

struct Instr {
  uint8_t op, dst, a, b, c;
  uint32_t imm = 0;
};

/** One point in, one record out. */
struct Point {
  int32_t in[kNin] = {0};
  int32_t out[kNout] = {0};
};

struct Bench {
  Vzhao_field_v2_front& d;

  explicit Bench(Vzhao_field_v2_front& dut) : d(dut) {
    d.rst_n = 0;
    d.prog_we_i = 0;
    d.pt_valid_i = 0;
    d.res_ready_i = 0;
    d.instr_count_i = 0;
    d.n_in_i = 0;
    d.n_out_i = 0;
    d.tbl_n_i = 0;
    d.tbl_x_i = 0;
    d.tbl_y_i = 0;
    d.tbl_dy_i = 0;
    for (int k = 0; k < kNout; ++k) d.out_reg_i[k] = 0;
    for (int k = 0; k < kNin; ++k) d.pt_lane_i[k] = 0;
    d.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(d);
    d.rst_n = 1;
    d.eval();
  }

  void load(const std::vector<Instr>& prog) {
    for (size_t i = 0; i < prog.size(); ++i) {
      d.prog_we_i = 1;
      d.prog_addr_i = static_cast<uint32_t>(i);
      d.prog_op_i = prog[i].op;
      d.prog_dst_i = prog[i].dst;
      d.prog_a_i = prog[i].a;
      d.prog_b_i = prog[i].b;
      d.prog_c_i = prog[i].c;
      d.prog_imm_i = prog[i].imm;
      d.eval();
      zhao::tick(d);
    }
    d.prog_we_i = 0;
    d.instr_count_i = static_cast<uint8_t>(prog.size());
    d.eval();
  }

  /** Push every point through and collect the answers, in arrival order. */
  std::vector<Point> run(const std::vector<Point>& pts, int n_in, int n_out, const int* out_regs,
                         int guard = 400000) {
    d.n_in_i = static_cast<uint8_t>(n_in);
    d.n_out_i = static_cast<uint8_t>(n_out);
    for (int k = 0; k < n_out; ++k) d.out_reg_i[k] = static_cast<uint32_t>(out_regs[k]);
    d.res_ready_i = 1;
    d.eval();

    std::vector<Point> got;
    size_t next = 0;
    int clocks = 0;
    while ((got.size() < pts.size()) && clocks < guard) {
      // present the next point whenever one is left
      const bool have = next < pts.size();
      d.pt_valid_i = have ? 1 : 0;
      if (have)
        for (int k = 0; k < kNin; ++k) d.pt_lane_i[k] = static_cast<uint32_t>(pts[next].in[k]);
      d.eval();

      const bool accepted = have && d.pt_ready_o;
      const bool emitted = d.res_valid_o != 0;
      Point r;
      if (emitted)
        for (int k = 0; k < n_out; ++k) r.out[k] = static_cast<int32_t>(d.res_lane_o[k]);

      zhao::tick(d);
      if (accepted) ++next;
      if (emitted) got.push_back(r);
      ++clocks;
    }
    d.pt_valid_i = 0;
    d.res_ready_i = 0;
    d.eval();
    std::printf("  front: %zu point(s) in %d clocks\n", got.size(), clocks);
    return got;
  }
};

/** The oracle: the reference interpreter, once per point, as terrain.cpp does. */
void oracle(const std::vector<Instr>& prog, const std::vector<Point>& pts, int n_in, int n_out,
            const int* out_regs, std::vector<Point>& want) {
  zfield::Decoded dec;
  dec.profile = 0;
  for (const Instr& in : prog) {
    zfield::Instr z{};
    z.op = to_ref(in.op);
    z.dst = in.dst;
    z.a = in.a;
    z.b = in.b;
    z.c = in.c;
    z.imm = in.imm;
    dec.instrs.push_back(z);
  }
  for (int k = 0; k < n_in; ++k) {
    zfield::IoLane il{};
    il.name = "i";
    il.type = 0;
    il.reg = static_cast<uint8_t>(k);
    dec.in_lanes.push_back(il);
  }
  for (int k = 0; k < n_out; ++k) {
    zfield::IoLane ol{};
    ol.name = "o";
    ol.type = 0;
    ol.reg = static_cast<uint8_t>(out_regs[k]);
    dec.out_lanes.push_back(ol);
  }
  want.clear();
  for (const Point& p : pts) {
    int32_t in[kNin];
    for (int k = 0; k < n_in; ++k) in[k] = p.in[k];
    int32_t out[kNout] = {0, 0, 0, 0};
    zfield::interpret(dec, in, static_cast<size_t>(n_in), out, static_cast<size_t>(n_out));
    Point r;
    for (int k = 0; k < n_out; ++k) r.out[k] = out[k];
    want.push_back(r);
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_field_v2_front dut;

  // A program with two inputs and two outputs, so the input map, the output map
  // and the register indices are all exercised at once. dst registers are
  // deliberately NOT 0/1 -- an output map that ignored out_reg_i and read the
  // first registers would pass if they were.
  const std::vector<Instr> prog = {
      {OP_ADD, 20, 0, 1, 0},  // r20 = a + b
      {OP_MUL, 21, 0, 1, 0},  // r21 = a * b   (Q16.16)
      {OP_END, 0, 0, 0, 0},
  };
  const int out_regs[2] = {20, 21};

  // ---- 1. one full batch -------------------------------------------------
  {
    Bench b(dut);
    b.load(prog);
    std::vector<Point> pts(kSlots);
    for (int i = 0; i < kSlots; ++i) {
      pts[static_cast<size_t>(i)].in[0] = (i + 1) * 1000;
      pts[static_cast<size_t>(i)].in[1] = (i * 7 + 3) * 601;
    }
    std::vector<Point> want;
    oracle(prog, pts, 2, 2, out_regs, want);
    const std::vector<Point> got = b.run(pts, 2, 2, out_regs);

    check(got.size() == pts.size(), "1.every point came back", pts.size(), got.size());
    uint64_t bad = 0;
    for (size_t i = 0; i < got.size() && i < want.size(); ++i)
      for (int k = 0; k < 2; ++k)
        if (got[i].out[k] != want[i].out[k]) ++bad;
    check(bad == 0, "1.a full batch matches zfield::interpret point for point", 0, bad);
  }

  // ---- 2. a PARTIAL batch, which is the normal case ----------------------
  // A patch is not a multiple of 32 points. A driver that waited for a full
  // batch would deadlock on the tail of every patch, and a driver that padded
  // one would return answers for points nobody asked about.
  {
    Bench b(dut);
    b.load(prog);
    const int kOdd = 5;
    std::vector<Point> pts(kOdd);
    for (int i = 0; i < kOdd; ++i) {
      pts[static_cast<size_t>(i)].in[0] = (i + 2) * 4099;
      pts[static_cast<size_t>(i)].in[1] = (i + 9) * 1301;
    }
    std::vector<Point> want;
    oracle(prog, pts, 2, 2, out_regs, want);
    const uint32_t before = dut.instr_retired_o;
    const std::vector<Point> got = b.run(pts, 2, 2, out_regs);

    check(got.size() == static_cast<size_t>(kOdd), "2.a partial batch returns exactly its points",
          kOdd, got.size());
    uint64_t bad = 0;
    for (size_t i = 0; i < got.size() && i < want.size(); ++i)
      for (int k = 0; k < 2; ++k)
        if (got[i].out[k] != want[i].out[k]) ++bad;
    check(bad == 0, "2.and matches the oracle", 0, bad);

    // AND IT MUST NOT EXECUTE WORK NOBODY ASKED FOR. A partial batch launches
    // only the wavefronts that actually hold points. Launching all of them
    // gives the same ANSWERS -- the extra slots are never drained -- so no value
    // check can see it. What it does do is run the program over stale registers,
    // burning cycles and feeding whatever is left there into the status and
    // saturation outputs.
    //
    // Mutant M155 survived every value check on exactly that. The retired
    // count is where it shows: 5 points fill ceil(5/4) = 2 wavefronts, and the
    // program is 3 instructions including END.
    const uint32_t retired = dut.instr_retired_o - before;
    const uint32_t want_retired = 2u * 3u;
    check(retired == want_retired, "2.a partial batch runs ONLY the wavefronts it filled",
          want_retired, retired);
  }

  // ---- 3. ORDER, on its own ----------------------------------------------
  // Every check above compares got[i] against want[i], so it would already
  // fail on a reordering -- unless the values happen to collide. This makes the
  // property explicit and unmissable: the first output lane is a strictly
  // increasing function of the point index, so the returned sequence must be
  // strictly increasing. Any permutation breaks it.
  {
    Bench b(dut);
    b.load(prog);
    const int kN = kSlots + 7;  // more than one batch, ending partial
    std::vector<Point> pts(static_cast<size_t>(kN));
    for (int i = 0; i < kN; ++i) {
      pts[static_cast<size_t>(i)].in[0] = (i + 1) << 8;  // strictly increasing
      pts[static_cast<size_t>(i)].in[1] = 1 << 16;       // r20 = a + 1.0
    }
    std::vector<Point> want;
    oracle(prog, pts, 2, 2, out_regs, want);
    const std::vector<Point> got = b.run(pts, 2, 2, out_regs);

    check(got.size() == static_cast<size_t>(kN), "3.multi-batch returned every point", kN,
          got.size());
    uint64_t bad = 0, disorder = 0;
    std::set<int32_t> seen;
    for (size_t i = 0; i < got.size(); ++i) {
      if (i < want.size() && got[i].out[0] != want[i].out[0]) ++bad;
      if (i > 0 && got[i].out[0] <= got[i - 1].out[0]) ++disorder;
      seen.insert(got[i].out[0]);
    }
    check(bad == 0, "3.every point matches across a batch boundary", 0, bad);
    check(disorder == 0, "3.points come back STRICTLY IN ORDER", 0, disorder);
    check(seen.size() == got.size(), "3.no point was returned twice", got.size(), seen.size());
  }

  // ---- 4. the program is what runs ---------------------------------------
  // Reload a DIFFERENT program into the same instance and require the answers
  // to change. Instruction memory that was written once and then ignored, or a
  // core reading a stale program, would pass every section above.
  {
    Bench b(dut);
    const std::vector<Instr> prog2 = {
        {OP_MUL, 20, 0, 0, 0},  // r20 = a * a
        {OP_ADD, 21, 0, 0, 0},  // r21 = a + a
        {OP_END, 0, 0, 0, 0},
    };
    b.load(prog2);
    std::vector<Point> pts(kSlots);
    for (int i = 0; i < kSlots; ++i) {
      pts[static_cast<size_t>(i)].in[0] = (i + 1) * 2731;
      pts[static_cast<size_t>(i)].in[1] = (i + 5) * 977;
    }
    std::vector<Point> want;
    oracle(prog2, pts, 2, 2, out_regs, want);
    const std::vector<Point> got = b.run(pts, 2, 2, out_regs);

    uint64_t bad = 0;
    for (size_t i = 0; i < got.size() && i < want.size(); ++i)
      for (int k = 0; k < 2; ++k)
        if (got[i].out[k] != want[i].out[k]) ++bad;
    check(bad == 0, "4.a second program gives the second program's answers", 0, bad);
    // and it really is a different computation
    std::vector<Point> want1;
    oracle(prog, pts, 2, 2, out_regs, want1);
    uint64_t same = 0;
    for (size_t i = 0; i < want.size(); ++i)
      if (want[i].out[0] == want1[i].out[0]) ++same;
    check(same < want.size(), "4.the two programs really do differ (oracle)", 0,
          same < want.size() ? 0 : 1);
  }

  // ---- 5. A PROGRAM WITH NO END MUST NOT HANG THE ENGINE ------------------
  // The front end's header claims a pc past `instr_count_i` is refused by the
  // core with ST_PC_OVERRUN rather than truncated here. That claim had no test
  // anywhere in the tree -- the core has the RTL, and nothing drove it -- so the
  // ledger's V20 rule was right to refuse the claim.
  //
  // This is the enforcement. A two-instruction program with NO END: every
  // wavefront runs off the end, the core must stop them with ST_PC_OVERRUN, and
  // the batch must still drain rather than wedge.
  {
    Bench b(dut);
    const std::vector<Instr> runaway = {
        {OP_ADD, 20, 0, 1, 0},
        {OP_MUL, 21, 0, 1, 0},
    };
    b.load(runaway);
    std::vector<Point> pts(kSlots);
    for (int i = 0; i < kSlots; ++i) {
      pts[static_cast<size_t>(i)].in[0] = (i + 1) * 313;
      pts[static_cast<size_t>(i)].in[1] = (i + 2) * 971;
    }
    const std::vector<Point> got = b.run(pts, 2, 2, out_regs, 100000);

    check(got.size() == pts.size(), "5.a program with no END still drains its batch", pts.size(),
          got.size());
    check(dut.status_o == 2, "5.and the core reports ST_PC_OVERRUN", 2, dut.status_o);
  }

  dut.final();
  return zhao::report_and_exit("field_v2_front_directed");
}
