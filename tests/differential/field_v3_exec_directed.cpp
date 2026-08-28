// field_v3_exec_directed.cpp — the differential for the Field v3 executor
// datapath (fpga/rtl/synth/zhao_probe_v3_exec.sv), Phase 4.
//
// THE ORACLE IS THE SHIPPED PLANNER, END TO END
// ----------------------------------------------
// Both sides run the SAME FPLAN. The test builds a canonical program, lowers
// it with `zfield::plan`, and then:
//
//   * the software side runs `zfield::prepare` + `zfield::execute_point`,
//     which is the reference the Phase 2 planner was swept 16/16 against;
//   * the hardware side has that same plan's uops loaded into its store and
//     the same point's inputs preloaded into its register file.
//
// So this is not "RTL against a model written next to it". It is RTL against
// the interpreter the whole Field IR is defined by.
//
// EVERYTHING IS VARYING, DELIBERATELY
// ------------------------------------
// `plan()` is given a varying mask covering every input lane, so no uniform
// elimination happens and every uop source is a VECTOR register. That is not
// a convenience: this increment has NO SCALAR BANK, and a plan that reached it
// with a `kSca` source would be executed against a register file that does not
// hold that value. The test ASSERTS the absence rather than assuming it, so
// the day the scalar bank arrives this check fails loudly instead of silently
// passing on a wrong read.
//
// WHAT IS CHECKED, AND WHY EACH ONE IS NOT REDUNDANT
// ---------------------------------------------------
//  1. THE OUTPUT VALUES, reconstructed from the WRITEBACK STREAM rather than
//     read out at the end. There is no host read port, and that turns out to
//     be the better test: every intermediate write is observed, so a program
//     that reaches the right answer through wrong intermediates is still
//     visible as a wrong write.
//  2. THE SATURATION LEDGER. `execute_point` reports the varying half's
//     ledger; the block ORs its own. A block that computes the right number
//     while lying about whether it clamped is wrong in the way that matters
//     later, when the ledger is what tells the game a value was pinned.
//  3. `desync_o` MUST STAY LOW. It latches if the multiplier's valid ever
//     fails to line up with S3 — meaning the product feeding the ALU belongs
//     to another instruction. That is a wrong answer, not a slow one.
//  4. `unsupported_o` IS CHECKED IN BOTH DIRECTIONS. Low for programs this
//     increment implements, and HIGH for a DOT program — because an op whose
//     products were never computed must be refused, not answered with zero.
//  5. THE BARREL PROPERTY, MEASURED. The datapath is five stages deep with one
//     instruction in flight per context, so one context alone can only issue
//     every fifth clock. Running eight contexts must fill the pipe. Both
//     numbers are measured rather than asserted, because "it should pipeline"
//     is exactly the claim that goes quietly wrong.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_probe_v3_engine.h"

#include "zfield/zfield.hpp"
#include "zfield/zfield_plan.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_sat.hpp"

namespace {

using zhao::check;

constexpr int kCtx = 8;
constexpr int kRegs = 32;
constexpr int kPlan = 32;

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint64_t next64() {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    uint64_t x = s;
    x ^= x >> 33;
    x *= 0xFF51AFD7ED558CCDULL;
    x ^= x >> 33;
    return x;
  }
  uint32_t below(uint32_t n) { return n ? (uint32_t)(next64() % n) : 0; }
  // Values that actually exercise the fx16 rails, not just the middle.
  int32_t interesting() {
    switch (below(8)) {
      case 0:
        return 0;
      case 1:
        return 1 << 16;
      case 2:
        return -(1 << 16);
      case 3:
        return INT32_MAX;
      case 4:
        return INT32_MIN;
      case 5:
        return INT32_MAX - (int32_t)below(4);
      case 6:
        return INT32_MIN + (int32_t)below(4);
      default:
        return (int32_t)next64();
    }
  }
};

// The ops this block implements. DOT2/DOT3 joined on 2026-08-28 when the
// sequencer landed -- they need two and three products against a
// one-multiplier-per-lane budget, so they hold the pipe while the extra
// products are collected. They are in the RANDOM set deliberately: the
// sequencer's interaction with ordinary ops around it is the part most likely
// to be wrong, and only mixed programs exercise it.
const uint8_t kOps[] = {zfield::OP_MOV,   zfield::OP_ADD,    zfield::OP_SUB, zfield::OP_MUL,
                        zfield::OP_MAD,   zfield::OP_MIN,    zfield::OP_MAX, zfield::OP_ABS,
                        zfield::OP_CLAMP, zfield::OP_SELECT, 0x10 /*DOT2*/,  0x11 /*DOT3*/};

// Build a canonical program over `n_in` varying lanes using only kOps.
// Maintains the validator's shape: def-before-use, dst outside the input
// lanes, exactly one END and it last.
zfield::Decoded alu_program(Prng& rng, int n_in, int n_body) {
  zfield::Decoded d;
  d.profile = 0;
  static const char* names[8] = {"x", "z", "p0", "p1", "p2", "p3", "p4", "p5"};
  for (int i = 0; i < n_in; ++i) {
    zfield::IoLane l;
    l.name = names[i];
    l.type = 0;
    l.reg = (uint8_t)i;
    d.in_lanes.push_back(l);
  }
  int next_reg = n_in;
  for (int k = 0; k < n_body && next_reg < 20; ++k) {
    zfield::Instr ins = {};
    ins.op = kOps[rng.below((uint32_t)(sizeof kOps))];

    // DOT2 and DOT3 read a GROUP of consecutive registers -- a, a+1 (and a+2)
    // -- not a single one. A group start chosen without room for its members
    // points the tail of the group at registers that were never defined, and
    // the planner then correctly lowers those members as SCALAR sources. That
    // is not an RTL fault and not a planner fault; it is an invalid program.
    // 114 of 400 randomized programs were being generated that way.
    const int width = (ins.op == 0x10) ? 2 : (ins.op == 0x11) ? 3 : 1;
    if (next_reg < width) {  // no room for the group yet
      ins.op = zfield::OP_ADD;
    }
    const int span = (ins.op == 0x10) ? 2 : (ins.op == 0x11) ? 3 : 1;
    const uint32_t hi = (uint32_t)(next_reg - span + 1);
    ins.dst = (uint8_t)next_reg;
    ins.a = (uint8_t)rng.below(hi);
    ins.b = (uint8_t)rng.below(hi);
    ins.c = (uint8_t)rng.below((uint32_t)next_reg);
    d.instrs.push_back(ins);
    ++next_reg;
  }
  zfield::Instr end = {};
  end.op = zfield::OP_END;
  d.instrs.push_back(end);

  zfield::IoLane o;
  o.name = "h";
  o.type = 0;
  o.reg = (uint8_t)(next_reg - 1);
  d.out_lanes.push_back(o);
  d.program_hash = 0xA10A0000u | (uint32_t)n_body;
  return d;
}

// ---------------------------------------------------------------------------

struct Dut {
  Vzhao_probe_v3_engine& t;
  // Shadow of the register file, rebuilt from the writeback stream.
  int32_t shadow[kCtx][kRegs] = {};

  explicit Dut(Vzhao_probe_v3_engine& top) : t(top) {}

  void reset() {
    t.rst_n = 0;
    t.up_we_i = 0;
    t.pre_we_i = 0;
    t.start_i = 0;
    t.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(t);
    t.rst_n = 1;
    t.eval();
    zhao::tick(t);
    std::memset(shadow, 0, sizeof shadow);
  }

  void load_uop(int ctx, int pc, uint8_t op, int dst, int a, int b, int c, uint32_t imm) {
    t.up_we_i = 1;
    t.up_ctx_i = (uint8_t)ctx;
    t.up_pc_i = (uint8_t)pc;
    t.up_op_i = op;
    t.up_dst_i = (uint8_t)dst;
    t.up_a_i = (uint8_t)a;
    t.up_b_i = (uint8_t)b;
    t.up_c_i = (uint8_t)c;
    t.up_imm_i = imm;
    zhao::tick(t);
    t.up_we_i = 0;
  }

  void preload(int ctx, int reg, int32_t v) {
    t.pre_we_i = 1;
    t.pre_ctx_i = (uint8_t)ctx;
    t.pre_reg_i = (uint8_t)reg;
    t.pre_data_i = (uint32_t)v;
    zhao::tick(t);
    t.pre_we_i = 0;
    shadow[ctx][reg] = v;
  }

  void start(int ctx) {
    t.start_i = 1;
    t.start_ctx_i = (uint8_t)ctx;
    zhao::tick(t);
    t.start_i = 0;
  }

  int writebacks = 0;  // how many writes the block has actually made

  // Advance one clock, folding any writeback into the shadow. Returns true if
  // a context finished this clock, and reports which.
  bool step(int* done_ctx) {
    const bool wb = t.wb_valid_o != 0;
    if (wb) ++writebacks;
    const int wctx = (int)t.wb_ctx_o, wreg = (int)t.wb_reg_o;
    const int32_t wdata = (int32_t)t.wb_data_o;
    const bool dn = t.done_valid_o != 0;
    if (done_ctx && dn) *done_ctx = (int)t.done_ctx_o;
    zhao::tick(t);
    if (wb) shadow[wctx][wreg] = wdata;
    return dn;
  }
};

// Load one plan into one context and preload that point's inputs.
// Returns false if the plan is outside this increment (a scalar source).
bool install(Dut& d, int ctx, const zfield::Fplan& fp, const int32_t* in, size_t n_in,
             bool* saw_scalar) {
  for (size_t u = 0; u < fp.uops.size(); ++u) {
    const zfield::VecUop& q = fp.uops[u];

    // THE FPLAN FLATTENS SOURCE GROUPS; THE HARDWARE WANTS GROUP STARTS.
    // zfield_plan lowers sources group by group -- `for g, for w:
    // src[k++] = starts[g] + w` -- so a DOT2 arrives as [a, a+1, b, b+1] and a
    // DOT3 as [a, a+1, a+2, b, b+1, b+2]. The register file reads a, a+1, a+2
    // from ONE address, so what it needs is the start of each group.
    //
    // The members are guaranteed consecutive after planning: compact_vregs is
    // order-preserving and notes that "group members are consecutive integers
    // and are all referenced, so nothing can fall between them". The hardware
    // depends on that, so it is ASSERTED below rather than assumed.
    const int aw = (q.op == 0x10) ? 2 : (q.op == 0x11) ? 3 : 1;
    int src[3] = {0, 0, 0};
    const int starts[3] = {0, aw, aw * 2};
    for (int g = 0; g < 3; ++g) {
      const int s = starts[g];
      if (s < (int)q.n_src) {
        if (q.src[s].kind != zfield::SrcKind::kVec) {
          *saw_scalar = true;
          return false;
        }
        src[g] = (int)q.src[s].idx;
      }
    }
    // Consecutiveness of every member within each group.
    for (int g = 0; g < 2; ++g) {
      for (int w = 1; w < aw; ++w) {
        const int s = starts[g] + w;
        if (s >= (int)q.n_src) continue;
        if (q.src[s].kind != zfield::SrcKind::kVec || (int)q.src[s].idx != src[g] + w) {
          *saw_scalar = true;  // not consecutive: outside what the RF can read
          return false;
        }
      }
    }
    d.load_uop(ctx, (int)u, q.op, (int)q.dst, src[0], src[1], src[2], q.imm);
  }
  d.load_uop(ctx, (int)fp.uops.size(), zfield::OP_END, 0, 0, 0, 0, 0);

  for (size_t l = 0; l < n_in && l < fp.in_vreg.size(); ++l) {
    if (fp.in_vreg[l] == 0xFF) continue;  // uniform: not this increment
    d.preload(ctx, (int)fp.in_vreg[l], in[l]);
  }
  return true;
}

// ---------------------------------------------------------------------------

// One program, one point, compared against execute_point.
void test_one_point_matches_the_interpreter(Vzhao_probe_v3_engine& top) {
  printf("-- one point against zfield::execute_point\n");
  Dut d(top);
  d.reset();

  Prng rng(0xC0FFEE01);
  const int n_in = 4;
  const zfield::Decoded prog = alu_program(rng, n_in, 10);
  const uint32_t vmask = (1u << n_in) - 1u;  // everything varies
  const zfield::Fplan fp = zfield::plan(prog, vmask);

  int32_t in[8] = {};
  for (int i = 0; i < n_in; ++i) in[i] = rng.interesting();

  bool saw_scalar = false;
  const bool ok = install(d, 0, fp, in, (size_t)n_in, &saw_scalar);
  check(!saw_scalar, "an all-varying plan has no scalar-bank sources", 0, saw_scalar ? 1 : 0);
  if (!ok) return;

  d.start(0);
  int done_ctx = -1;
  int guard = 0;
  while (guard++ < 4000) {
    if (d.step(&done_ctx) && done_ctx == 0) break;
  }
  check(guard < 4000, "the context reached END", 1, guard < 4000 ? 1 : 0);
  check(top.exec_desync_o == 0, "the multiplier stayed in step with the pipeline", 0,
        (int)top.exec_desync_o);
  check(top.unsupported_o == 0, "every op in the program is implemented", 0,
        (int)top.unsupported_o);

  // The oracle.
  const zfield::Prepared prep = zfield::prepare(fp, prog, in, (size_t)n_in);
  int32_t want[4] = {};
  zref::SatLedger led;
  zfield::execute_point(fp, prog, prep, in, (size_t)n_in, want, fp.out_map.size(), &led);

  bool all = true;
  for (size_t o = 0; o < fp.out_map.size(); ++o) {
    if (fp.out_map[o].kind != zfield::SrcKind::kVec) continue;
    const int32_t got = d.shadow[0][fp.out_map[o].idx];
    if (got != want[o]) {
      printf("   out lane %zu: want %d got %d\n", o, want[o], got);
      all = false;
    }
  }
  check(all, "every output register matches the interpreter", 1, all ? 1 : 0);

  check(top.sat_add_o == (led.add != 0), "the ADD saturation flag matches the ledger", led.add != 0,
        (int)top.sat_add_o);
  check(top.sat_mul_o == (led.mul != 0), "the MUL saturation flag matches the ledger", led.mul != 0,
        (int)top.sat_mul_o);
}

// A DOT program must be REFUSED, not answered with a zero product.
void test_dot_is_refused_not_answered(Vzhao_probe_v3_engine& top) {
  printf("-- DOT is now COMPUTED, and an unknown opcode is still refused\n");
  Dut d(top);
  d.reset();
  d.preload(0, 0, 3 << 16);
  d.preload(0, 1, 5 << 16);
  d.preload(0, 2, 7 << 16);
  d.preload(0, 3, 11 << 16);
  d.load_uop(0, 0, 0x10 /* OP_DOT2 */, 8, 0, 2, 0, 0);
  d.load_uop(0, 1, zfield::OP_END, 0, 0, 0, 0, 0);
  d.start(0);
  int done_ctx = -1;
  int guard = 0;
  while (guard++ < 200) {
    if (d.step(&done_ctx) && done_ctx == 0) break;
  }
  // THIS ASSERTION INVERTED ON 2026-08-28, and that inversion is the point:
  // reports/FIELD_V3_DOT_SEQUENCING.md predicted it as the signal that the
  // RTL's scope note and this test moved together rather than one drifting
  // from the other. DOT is implemented; it must be COMPUTED, not refused.
  check(top.unsupported_o == 0, "a DOT op is implemented now, not refused", 0,
        (int)top.unsupported_o);
  check(top.exec_desync_o == 0, "and it does not desynchronise the pipeline", 0,
        (int)top.exec_desync_o);
  // 3*7 + 5*11 in fx16 = (3<<16)*(7<<16)>>16 + ... -- the value is checked
  // against the interpreter by the randomized lane; here the point is that a
  // DOT PRODUCES a write at all, which the refusing version never did.
  check(d.writebacks > 0, "and it writes its result", 1, d.writebacks > 0 ? 1 : 0);

  // The "a refusal writes nothing" check that used to live here moved to the
  // UNKNOWN-OPCODE case below when DOT became implemented. It was never about
  // DOT specifically -- it is about the ALU's own `default` refusal, which is
  // the path `alu_writes` actually gates.

  // THERE ARE TWO REFUSAL PATHS AND THEY ARE NOT THE SAME GATE. A DOT is
  // refused by THIS block (dot_here_c), because the ALU knows the opcode and
  // would happily write. An opcode the ALU does not know at all is refused by
  // the ALU's own `default` arm, which clears writes_o -- and `alu_writes` is
  // then the only thing stopping the write.
  //
  // X11 dropped `alu_writes` and SURVIVED the re-score, because after the
  // dot_here_c fix every op the test drove was either a DOT (blocked by the
  // new term) or an op the ALU implements. Covering one path had hidden the
  // other. An unknown opcode drives the second one.
  // AND THE STRAY WRITE MUST BE READ BACK, not inferred from the valid port.
  // The write enable and the observation port are SEPARATELY GATED --
  // `rf_we_c` drives the file, `wb_valid_o` drives what the test sees -- so a
  // mutation to one is invisible through the other. X11 survived twice on
  // exactly that: it dropped `alu_writes` from the write enable only, wrote
  // into the register file, and left the valid port silent.
  //
  // The only way to observe the file is to READ IT with a following
  // instruction. r4 is seeded with a sentinel; the refused op targets r4; a
  // MOV then copies r4 out where the writeback stream can be seen. If the
  // refusal wrote, the sentinel is gone.
  constexpr int32_t kSentinel = 0x00BEEF00;
  Dut u(top);
  u.reset();
  u.preload(0, 0, 5 << 16);
  u.preload(0, 4, kSentinel);
  u.load_uop(0, 0, 0x2A /* not a canonical opcode */, 4, 0, 0, 0, 0);
  u.load_uop(0, 1, zfield::OP_MOV, 5, 4, 0, 0, 0);
  u.load_uop(0, 2, zfield::OP_END, 0, 0, 0, 0, 0);
  u.start(0);
  int udone = -1, uguard = 0;
  while (uguard++ < 200) {
    if (u.step(&udone) && udone == 0) break;
  }
  check(top.unsupported_o == 1, "an opcode the ALU does not know is refused too", 1,
        (int)top.unsupported_o);
  check(u.shadow[0][5] == kSentinel,
        "the refused op left the register FILE untouched, read back through a MOV",
        (uint64_t)(uint32_t)kSentinel, (uint64_t)(uint32_t)u.shadow[0][5]);
}

// EACH SATURATION LANE, ALONE. The first sweep survived two mutants that
// widened a flag's source -- the ADD flag also latching on a MUL clamp (X16)
// and the MUL flag also latching on a rescale clamp (X17). Nothing caught
// them because the only ledger comparison ran on a random program where
// several lanes fired together, and a flag that is too eager is invisible
// beside a flag that should be set anyway.
//
// So each lane gets a program that fires THAT lane and no other, and all
// three flags are checked every time -- including sat_rescale, which no test
// read at all until X17 pointed at it. OP_ABS is the reachable rescale: the
// ALU sets sat_rescale_o from abs_sat_fired, and |INT32_MIN| is off the rail.
void test_each_saturation_lane_alone(Vzhao_probe_v3_engine& top) {
  printf("-- each saturation lane fires alone\n");
  struct Case {
    const char* what;
    uint8_t op;
    int32_t a, b;
    int add, mul, resc;
  };
  const Case cases[3] = {
      {"ADD", zfield::OP_ADD, INT32_MAX, INT32_MAX, 1, 0, 0},
      {"MUL", zfield::OP_MUL, INT32_MAX, INT32_MAX, 0, 1, 0},
      {"ABS", zfield::OP_ABS, INT32_MIN, 0, 0, 0, 1},
  };
  for (const Case& c : cases) {
    Dut d(top);
    d.reset();  // the flags are sticky, so each case needs a clean block
    d.preload(0, 0, c.a);
    d.preload(0, 1, c.b);
    d.load_uop(0, 0, c.op, 4, 0, 1, 0, 0);
    d.load_uop(0, 1, zfield::OP_END, 0, 0, 0, 0, 0);
    d.start(0);
    int done = -1, guard = 0;
    while (guard++ < 200) {
      if (d.step(&done) && done == 0) break;
    }
    char msg[128];
    snprintf(msg, sizeof msg, "%s saturates: the ADD flag is %d", c.what, c.add);
    check((int)top.sat_add_o == c.add, msg, c.add, (int)top.sat_add_o);
    snprintf(msg, sizeof msg, "%s saturates: the MUL flag is %d", c.what, c.mul);
    check((int)top.sat_mul_o == c.mul, msg, c.mul, (int)top.sat_mul_o);
    snprintf(msg, sizeof msg, "%s saturates: the RESCALE flag is %d", c.what, c.resc);
    check((int)top.sat_rescale_o == c.resc, msg, c.resc, (int)top.sat_rescale_o);
  }
}

// The barrel property, measured on both sides of it.
void test_barrel_occupancy(Vzhao_probe_v3_engine& top) {
  printf("-- the barrel: one context stalls, eight fill the pipe\n");
  Prng rng(0xBA22E1);
  const int n_in = 4;
  const zfield::Decoded prog = alu_program(rng, n_in, 12);
  const zfield::Fplan fp = zfield::plan(prog, (1u << n_in) - 1u);
  int32_t in[8] = {};
  for (int i = 0; i < n_in; ++i) in[i] = rng.interesting();

  // One context alone.
  Dut d1(top);
  d1.reset();
  bool sc = false;
  if (!install(d1, 0, fp, in, (size_t)n_in, &sc)) return;
  d1.start(0);
  int done = -1, clocks_1 = 0;
  while (clocks_1++ < 4000) {
    if (d1.step(&done) && done == 0) break;
  }
  const uint32_t issued_1 = top.uops_issued_o;

  // Eight contexts together.
  Dut d8(top);
  d8.reset();
  for (int c = 0; c < kCtx; ++c) {
    if (!install(d8, c, fp, in, (size_t)n_in, &sc)) return;
  }
  for (int c = 0; c < kCtx; ++c) d8.start(c);
  int finished = 0, clocks_8 = 0;
  while (clocks_8++ < 8000 && finished < kCtx) {
    int dc = -1;
    if (d8.step(&dc)) ++finished;
  }
  const uint32_t issued_8 = top.uops_issued_o;

  printf("   MEASURED: 1 context = %u uops in %d clocks; 8 contexts = %u uops in %d clocks\n",
         issued_1, clocks_1, issued_8, clocks_8);
  check(finished == kCtx, "all eight contexts finished", kCtx, finished);
  check(top.exec_desync_o == 0, "the multiplier stayed in step throughout", 0,
        (int)top.exec_desync_o);

  // THE CYCLE COUNTS ARE PINNED, and that is the point of measuring them.
  // X05 -- releasing a context for re-issue one stage early -- SURVIVED the
  // first sweep. It is harmless for VALUES (the write lands before the
  // re-issued read can reach the file) but it changes OCCUPANCY, and nothing
  // was looking at occupancy: the only timing check was the loose inequality
  // below. A pipeline whose depth silently changes is a pipeline whose budget
  // is no longer the one the deadline was computed from.
  //
  // These are MEASURED on this program, not targets. If a change moves them,
  // that is a fact to look at and re-pin, not a test to relax.
  //
  // RE-PINNED 2026-08-28 when DOT sequencing landed and DOT joined the random
  // op set. The movement is the finding:
  //
  //     one context    65 -> 66 clocks   (+1)
  //     eight contexts 126 -> 166 clocks (+32%)
  //
  // One context barely notices: it is already stalled by the five-stage depth,
  // so the DOT hold overlaps with waiting it was doing anyway. Eight contexts
  // pay 32%, and that asymmetry is the honest cost of the design choice --
  // a DOT freezes ISSUE GLOBALLY, so one context's dot product stalls all
  // seven others. That bought a sequencer with no multiplier arbiter.
  //
  // The alternative -- stalling only the DOT's own context and letting the
  // others issue -- needs an arbiter on the multiplier port. Whether it is
  // worth it depends on DOT density in real Earth programs, which the Phase 4
  // composition test measures and this microbenchmark cannot.
  // RE-PINNED 2026-08-28 when the DOT sequence moved entirely to S4:
  //
  //     one context    66 -> 69 clocks   (+4.5%)
  //     eight contexts 166 -> 190 clocks (+14%)
  //
  // That is the price of correctness under contention, and it is worth naming
  // precisely. The old schedule issued a DOT's products from S2, S3 and S4 --
  // overlapping them with the instruction's own progress, which is why it was
  // cheaper. It was also unfixable: an instruction cannot be stalled between
  // its multiply issue and its product arrival, so any refusal broke it.
  //
  // Issuing all products from S4 costs those extra clocks and cannot miss a
  // product, because the operands do not move for the whole sequence.
  check(clocks_1 == 69, "one context: the measured cost, pinned", 69, clocks_1);
  check(clocks_8 == 190, "eight contexts: the measured cost, pinned", 190, clocks_8);

  // Eight contexts do eight times the work in far less than eight times the
  // clocks -- that IS the barrel. Stated as a measured inequality rather than
  // a target, because the depth is what it is and the point is to see it.
  check(clocks_8 < clocks_1 * kCtx, "eight contexts cost less than eight serial runs",
        clocks_1 * kCtx, clocks_8);
}

// THE RIVAL MUST ACTUALLY ASK, or half this block is untested.
//
// The engine carries a `rival_req_i` port for one reason: the executor shares
// the multiplier bank, and the ONLY new behaviour the composition creates is
// being REFUSED. With the rival silent the bank always grants, no request is
// ever denied, and every path that handles denial is dead code.
//
// I added that port for exactly this purpose and then wrote a test that left
// it at zero. The first engine sweep scored FIVE survivors and every one of
// them lived in the rival path -- the denial logic, the reply routing, the
// operand wiring. None of them was reachable.
//
// So the rival now asks on a pseudo-random schedule while a real program
// runs. The program's outputs must STILL match the interpreter: a refusal is
// allowed to cost clocks and is not allowed to change an answer.
void test_results_survive_contention(Vzhao_probe_v3_engine& top, int programs) {
  printf("-- the rival contends; answers must not move\n");
  Prng rng(0xC047E17);
  int bad = 0, ran = 0;
  uint32_t stalls_seen = 0;

  for (int k = 0; k < programs; ++k) {
    Dut d(top);
    d.reset();
    const int n_in = 4;
    const zfield::Decoded prog = alu_program(rng, n_in, 8 + (int)rng.below(8));
    const zfield::Fplan fp = zfield::plan(prog, (1u << n_in) - 1u);
    if (fp.uops.size() + 1 >= (size_t)kPlan) continue;

    int32_t in[8] = {};
    for (int i = 0; i < n_in; ++i) in[i] = rng.interesting();
    bool sc = false;
    if (!install(d, 0, fp, in, (size_t)n_in, &sc)) continue;

    d.start(0);
    int done = -1, guard = 0;
    while (guard++ < 8000) {
      // The rival presses the bank about half the time. It outranks the
      // executor, so this genuinely denies the executor's requests.
      top.rival_req_i = (rng.below(2) != 0) ? 1 : 0;
      if (d.step(&done) && done == 0) break;
    }
    top.rival_req_i = 0;
    ++ran;
    stalls_seen = top.lane_stalls_o;

    const zfield::Prepared prep = zfield::prepare(fp, prog, in, (size_t)n_in);
    int32_t want[4] = {};
    zfield::execute_point(fp, prog, prep, in, (size_t)n_in, want, fp.out_map.size(), nullptr);
    for (size_t o = 0; o < fp.out_map.size(); ++o) {
      if (fp.out_map[o].kind != zfield::SrcKind::kVec) continue;
      if (d.shadow[0][fp.out_map[o].idx] != want[o]) {
        ++bad;
        break;
      }
    }
  }

  printf("   MEASURED: %d programs under contention, %u lane stalls on the last\n", ran,
         stalls_seen);
  check(ran > 0, "programs actually ran under contention", 1, ran > 0 ? 1 : 0);
  // The whole point: refusals must HAPPEN, or this test proves nothing.
  check(stalls_seen > 0, "the executor was actually refused the bank", 1, stalls_seen > 0 ? 1 : 0);
  check(bad == 0, "every answer survives contention unchanged", 0, bad);
  check(top.exec_desync_o == 0, "and the pipeline stayed in step", 0, (int)top.exec_desync_o);
  check(top.bank_desync_o == 0, "and so did the bank", 0, (int)top.bank_desync_o);
}

// Randomized: many programs, many points.
void test_random(Vzhao_probe_v3_engine& top, int iters) {
  printf("-- randomized differential, %d programs\n", iters);
  Prng rng(0x5A1AD5);
  int bad = 0, scalar_plans = 0, ran = 0;
  for (int k = 0; k < iters; ++k) {
    Dut d(top);
    d.reset();
    const int n_in = 2 + (int)rng.below(4);
    const zfield::Decoded prog = alu_program(rng, n_in, 4 + (int)rng.below(12));
    const zfield::Fplan fp = zfield::plan(prog, (1u << n_in) - 1u);
    if (fp.uops.size() + 1 >= (size_t)kPlan) continue;

    int32_t in[8] = {};
    for (int i = 0; i < n_in; ++i) in[i] = rng.interesting();

    bool sc = false;
    if (!install(d, 0, fp, in, (size_t)n_in, &sc)) {
      ++scalar_plans;
      continue;
    }
    d.start(0);
    int done = -1, guard = 0;
    while (guard++ < 4000) {
      if (d.step(&done) && done == 0) break;
    }
    ++ran;

    const zfield::Prepared prep = zfield::prepare(fp, prog, in, (size_t)n_in);
    int32_t want[4] = {};
    zfield::execute_point(fp, prog, prep, in, (size_t)n_in, want, fp.out_map.size(), nullptr);
    for (size_t o = 0; o < fp.out_map.size(); ++o) {
      if (fp.out_map[o].kind != zfield::SrcKind::kVec) continue;
      if (d.shadow[0][fp.out_map[o].idx] != want[o]) {
        ++bad;
        break;
      }
    }
  }
  printf("   %d programs executed, %d skipped for scalar sources\n", ran, scalar_plans);
  check(ran > 0, "some programs actually ran", 1, ran > 0 ? 1 : 0);
  check(scalar_plans == 0, "an all-varying plan never produces a scalar source", 0, scalar_plans);
  check(bad == 0, "every randomized program matches the interpreter", 0, bad);
  check(top.exec_desync_o == 0, "the multiplier stayed in step throughout", 0,
        (int)top.exec_desync_o);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  int iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--random" && i + 1 < argc) iters = std::atoi(argv[++i]);
  }

  Vzhao_probe_v3_engine top;

  if (iters > 0) {
    test_random(top, iters);
  } else {
    test_one_point_matches_the_interpreter(top);
    test_dot_is_refused_not_answered(top);
    test_each_saturation_lane_alone(top);
    test_barrel_occupancy(top);
    test_results_survive_contention(top, 12);
  }
  return zhao::report_and_exit("FIELD.V3.EXEC");
}
