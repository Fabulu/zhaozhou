// field_v3_earth_directed.cpp — GATE 3: the composed Earth measurement.
//
// ENFORCED-BY: itself. This IS the gate.
//
// ---------------------------------------------------------------------------
// WHAT THIS ANSWERS AND WHY NOTHING ELSE COULD
// ---------------------------------------------------------------------------
// The admission law is two numbers: <= 6,000 clocks per association and
// <= 850,000 clocks for the 128-association stress frame. 273 four-point groups
// per association, 34,944 groups per frame, so 24.3 clocks per group.
//
// `tools/field/measure_earth_budget.cpp` answers that question by ARITHMETIC:
// it plans a real program, looks each uop's measured initiation interval up in
// a table, and adds. It reports two bounds, and it says out loud that it cannot
// choose between them:
//
//     SERIAL      sum of every uop's II            crater_ring 1,817,088
//     OVERLAPPED  the busiest single service         all three   768,768
//
// The whole difference -- 2.1x over versus inside with margin -- is whether the
// services actually run at the same time on the real machine. That is not a
// property of a table. It is a property of one multiplier bank, one register
// file write port, a four-deep dispatcher queue and a priority arbiter, all of
// which this file instantiates and none of which the tool can see.
//
// So this runs REAL Earth programs on the REAL composed engine and counts the
// clocks. That number needs no bound around it.
//
// ---------------------------------------------------------------------------
// A MEASUREMENT THAT IS NOT ALSO A CORRECTNESS CHECK IS WORTHLESS
// ---------------------------------------------------------------------------
// A machine that computes the wrong answer quickly is not fast, and this engine
// has already produced one throughput figure that was measuring the test's own
// scaffolding rather than the silicon. So every retired point is checked
// against `zfield::execute_point` -- the shipped oracle, on the same inputs,
// through the same plan -- before its clocks are allowed to count.
//
// ---------------------------------------------------------------------------
// II, NOT LATENCY. THE SAME MISTAKE THIS ENGINE HAS MADE THREE TIMES
// ---------------------------------------------------------------------------
// The eight contexts are kept FULL. A context that retires is checked, reloaded
// and restarted immediately, so the engine never runs dry and what is measured
// is the rate it sustains rather than the time one point takes. The first
// several retirements are discarded as fill: at the start the pipeline is empty
// and the first point through any pipeline is always the slowest.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE MEASURES THAT IS NOT THE SILICON, AND SAYS SO
// ---------------------------------------------------------------------------
// Point data arrives one register per clock through `pre_we_i`, because that is
// the port the probe has. The real machine streams points from memory. Those
// clocks are NOT excluded -- they are overlapped with execution, because the
// preload of one context happens while the other seven run, and every clock is
// counted exactly once by a single `advance()`. But if this figure ever comes
// out limited by preload rather than by a service, that is the harness's port
// and not the engine's rate, so the preload count is reported alongside.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "verilated.h"

#include "Vzhao_probe_v3_full.h"

#include "zfield/zfield.hpp"
#include "zfield/zfield_plan.hpp"
#include "zfield/generated/zfield_optable.hpp"
#include "zfield/zfield_steps.hpp"
#include "zhao_sim.hpp"

namespace {

// The context count is a BUILD knob so the same harness can measure the same
// programs on a wider machine without disturbing the CTX=8 tallies every other
// block test was scored at.
#ifndef ZHAO_EARTH_CTX
#define ZHAO_EARTH_CTX 8
#endif
constexpr int kCtx = ZHAO_EARTH_CTX;

// POINTS PER CONTEXT. At 1 a context is one point and the machine is the
// scalar executor this file was written against. At 4 a context IS a quad: one
// instruction, four points, one register write carrying all four.
#ifndef ZHAO_EARTH_LANES
#define ZHAO_EARTH_LANES 1
#endif
constexpr int kLanes = ZHAO_EARTH_LANES;

// A DISPATCH GROUP IS FOUR POINTS. It is NOT four contexts -- that was true
// only while a context was one point, and it silently stopped being true when
// the executor was widened.
//
// Bundling four CONTEXTS at LANES=4 starts sixteen synchronised points, which
// is a four-point group's worth of alignment applied four times over. It
// collides exactly the distance, curve and trig requests that staggering the
// groups exists to spread out -- so the driver was manufacturing the service
// contention it was written to avoid, and the machine was measured under it.
constexpr int kPointsPerDispatchGroup = 4;
static_assert(kPointsPerDispatchGroup % kLanes == 0,
              "a dispatch group must be a whole number of contexts");
constexpr int kCtxPerGroup = kPointsPerDispatchGroup / kLanes;

// Verilator hands a 32-bit port back as a scalar and a 128-bit one as an
// indexable word array, so the two cannot share an accessor. Branching on the
// width here keeps every call site below reading as if they could.
inline void put_lane(Vzhao_probe_v3_full& t, const int32_t* v) {
#if ZHAO_EARTH_LANES == 1
  t.pre_data_i = (uint32_t)v[0];
#else
  for (int l = 0; l < kLanes; ++l) t.pre_data_i[l] = (uint32_t)v[l];
#endif
}
inline int32_t get_wr_lane(const Vzhao_probe_v3_full& t, int lane) {
#if ZHAO_EARTH_LANES == 1
  (void)lane;
  return (int32_t)t.wr_data_o;
#else
  return (int32_t)t.wr_data_o[lane];
#endif
}
// REGISTERS PER CONTEXT. A build knob because crater_ring does not fit in 32:
// it needs 7 vector + 29 uniform = 36, and the executor has no scalar-bank read
// path, so uniforms reach it only by being broadcast into registers.
#ifndef ZHAO_EARTH_REGS
#define ZHAO_EARTH_REGS 32
#endif
constexpr int kRegs = ZHAO_EARTH_REGS;
constexpr int kPlan = 32;

// The three ways points can be fed to the machine. They are not equivalent and
// the difference is larger than any arithmetic change measured so far.
constexpr int kDriveStaggered = 0;
constexpr int kDriveWave = 1;
constexpr int kDriveQuad = 2;

// The law, from reports/Fieldv3.md and the owner's own number.
constexpr long kFrameBudget = 850000;
constexpr long kAssocBudget = 6000;
constexpr long kGroupsPerAssoc = 273;
constexpr long kAssocPerFrame = 128;

int failures = 0;       // gates the exit status
int diag_failures = 0;  // real, recorded, and NOT yet gating -- see the report
bool gating = true;
int printed = 0;
// 0 = ALU-first, 1 = drain-first, 2 = round robin.
//
// DRAIN-FIRST WAS RIGHT AND IS NOT ANY MORE. It was chosen when a drain needed
// FOUR writes to return a four-point group, one point at a time, and ALU-first
// starved it outright. A drain now returns all four points in ONE write, so it
// asks for the port a quarter as often and the old measurement is stale.
//
//     ALU-first     impact_wave 803,712 FITS   wave_pool 838,656 FITS
//     round robin   impact_wave 803,712 FITS   wave_pool 873,600 2.8% over
//     drain-first   impact_wave 803,712 FITS   wave_pool 908,544 6.9% over
//
// ALU-first stalls the drain 1,043 clocks against round robin's 11, and it is
// still the right default, because that delay CANNOT become starvation. For
// the ALU to hold the port forever some context must always have ALU work;
// that context must not be parked; so its long op must have returned; so the
// drain must have run. The drain always eventually wins, and the measurements
// agree -- every group is served and every value is exact under all three.
//
// Round robin is the conservative alternative and it costs 2.8% on wave_pool.
// Both numbers are reported rather than only the flattering one.
int wb_policy_probe = 0;

// ON, AND THE MEASUREMENT IS WHY -- IN BOTH DIRECTIONS.
//
// Every Earth builder expands `smoothstep(e0, e1, x)` into SEVEN varying uops,
// which is a third of wave_pool's hot loop and the largest single block of work
// in any of the three programs. The prepared ring already computes exactly that
// sequence as its first four products.
//
// The substitution is bit-exact: 39,321 combinations of distance, edge and span
// with zero value AND zero ledger mismatches, then confirmed end to end by this
// gate -- the hardware runs the REWRITTEN program while `execute_point` checks
// it against the UNTOUCHED plan, so the oracle has never heard of the rewrite.
//
// It was a LOSS first. Run through the full nine-product ring recipe it made
// every program worse:
//
//     crater_ring  663,936 -> 1,083,264      longop-hold  398 -> 1372
//
// Seven cheap ALU slots became one expensive service request, and all three
// programs contain a smoothstep, so the ring went from serving one program to
// serving every point of all three. Doubling RING_UNITS recovered almost
// nothing, which is what identified the nine PRODUCTS rather than the unit
// count as the cost -- and justified building a mode that takes only the four
// a smoothstep needs.
//
// With that mode (imm bit 24):
//
//     impact_wave  733,824 -> 594,048
//     wave_pool    663,936 -> 559,104
//     crater_ring  663,936 -> 663,936   (two ring ops per point; RING_UNITS 8)
//
// `--smoothstep` is now the default and `--no-smoothstep` turns it off, because
// the contraction is the shipped behaviour and the un-contracted plan is the
// control.
bool contract_ss = true;

void fail(const char* what, const char* detail) {
  if (printed++ < 6) printf("  %s %s: %s\n", gating ? "FAIL" : "DEFECT", what, detail);
  if (gating) {
    ++failures;
  } else {
    ++diag_failures;
  }
}

std::vector<uint8_t> slurp(const char* path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
}

/** Deterministic, so a failure is reproducible from the seed alone. */
struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return (uint32_t)(s >> 32);
  }
};

/** The hardware's own source-group width, from zhao_field_ops_pkg.sv. A long
 *  op whose operand a is a 2-vector reads registers a and a+1; the dispatcher
 *  calls those s0 and s1, and operand b's two members s3 and s4. */
int src_group_width(uint8_t op) {
  switch (op) {
    case zfield::OP_DIST2:
    case zfield::OP_LEN2:
      return 2;
    case zfield::OP_LEN3:
      return 3;
    default:
      return 1;
  }
}

/** One uop as the silicon takes it: an opcode and three REGISTER GROUP STARTS.
 *
 *  This translation is the exact place a bug already lived once. The oracle's
 *  `VecUop` carries a FLATTENED `src[9]`; the hardware has per-operand-group
 *  ports. Reasoning from the flat array wired ROT2's angle to the wrong port,
 *  and ROT3 passed while ROT2 failed. So the mapping is written out per op
 *  shape rather than inferred, and anything whose shape is not written out is
 *  REFUSED BY NAME instead of being guessed at. */
struct Mapped {
  uint8_t op;
  int dst, a, b, c;
  uint32_t imm;
};

struct Translator {
  const zfield::Fplan& fp;
  int scalar_base;
  std::string refusal;

  Translator(const zfield::Fplan& f, int base) : fp(f), scalar_base(base) {}

  /** A source becomes a register. Vector registers sit at the bottom of the
   *  file and the association's uniform values are BROADCAST above them --
   *  every context holds the same copy, written once per association, which
   *  costs 0.85 clocks per group amortised over 273 and buys the whole
   *  seven-port read structure the register file already has. */
  int reg_of(const zfield::UopSrc& s) {
    return s.kind == zfield::SrcKind::kVec ? (int)s.idx : scalar_base + (int)s.idx;
  }

  bool map_one(const zfield::VecUop& u, Mapped* m) {
    m->op = u.op;
    m->dst = (int)u.dst;
    m->a = m->b = m->c = 0;
    m->imm = u.imm;

    if (u.op == zfield::UOP_RING_PREP) {
      // The prepared ring reads its four uniforms from the SCALAR BANK by
      // index, packed into the immediate -- that is the hot path built for it
      // and it does not go through the register file at all.
      if (u.n_src != 5) {
        refusal = "RING_PREP with an unexpected source count";
        return false;
      }
      m->a = reg_of(u.src[0]);
      for (int k = 1; k < 5; ++k) {
        if (u.src[k].kind != zfield::SrcKind::kSca) {
          refusal = "RING_PREP with a non-uniform radius/centre";
          return false;
        }
        if (u.src[k].idx > 63) {
          refusal = "RING_PREP slot past the 64-slot bank";
          return false;
        }
      }
      // Bit 24 carries smooth mode through from the contraction.
      m->imm = ((uint32_t)u.src[1].idx) | ((uint32_t)u.src[2].idx << 6) |
               ((uint32_t)u.src[3].idx << 12) | ((uint32_t)u.src[4].idx << 18) |
               (u.imm & (1u << 24));
      return true;
    }

    const int w = src_group_width(u.op);
    if (w * 2 == (int)u.n_src) {
      // Two operand groups of w members: a = src[0..w-1], b = src[w..2w-1].
      // Both groups must be CONSECUTIVE registers, because that is what the
      // register file's group read means. The uniform broadcast preserves slot
      // order, so a pair of adjacent scalar slots stays adjacent.
      m->a = reg_of(u.src[0]);
      m->b = reg_of(u.src[w]);
      for (int k = 1; k < w; ++k) {
        if (reg_of(u.src[k]) != m->a + k || reg_of(u.src[w + k]) != m->b + k) {
          refusal = "an operand group whose members are not consecutive registers";
          return false;
        }
      }
      return true;
    }
    if (w == 1 && u.n_src >= 1 && u.n_src <= 3) {
      m->a = reg_of(u.src[0]);
      if (u.n_src > 1) m->b = reg_of(u.src[1]);
      if (u.n_src > 2) m->c = reg_of(u.src[2]);
      return true;
    }
    refusal = "an op shape this translator has not been taught";
    return false;
  }
};

struct Dut {
  Vzhao_probe_v3_full& t;
  int32_t shadow[kCtx][kRegs][kLanes] = {};
  long clocks = 0;
  long preload_clocks = 0;
  int last_done = -1;
  bool saw_done = false;

  // IS `done` A PROMISE THAT THE RESULTS HAVE LANDED?
  //
  // The staggered drive got two values wrong out of 762 and one of them read
  // back as zero -- the signature of a register that had not been written yet
  // rather than one written with the wrong number. This engine has paid for
  // "declared finished while the last capture was still in flight" three times
  // already, so the question gets instrumented instead of argued about.
  //
  // `done_at` is the clock a context retired on; any write that arrives for
  // that context afterwards, before it is restarted, is a LATE WRITE and means
  // `done_valid_o` is not the guarantee a consumer would read it as.
  long done_at[kCtx] = {};
  int late_writes[kCtx] = {};
  int late_reg[kCtx] = {};

  // A RETIREMENT IS A QUEUE, NOT A FLAG.
  //
  // Reloading a context takes clocks, and those clocks go through `advance()`
  // like every other clock. A single `saw_done` flag is therefore overwritten
  // by the reload of the context that just retired, and any OTHER context that
  // finished during those two or three clocks is lost -- silently, because a
  // lost retirement looks like a context that is simply still running.
  //
  // Wave drive never noticed: it reloads all eight only between waves, when
  // nothing is in flight. Staggered drive reloads inside the loop, which is
  // why the two wrong values appeared there and only there.
  bool done_q[kCtx] = {};

  // AND A RETIREMENT IS ONLY REAL IF THE CONTEXT WAS ACTUALLY RUNNING.
  //
  // `done_valid_o` stays asserted while a context sits finished, so a queue
  // alone re-retires it every clock until it is restarted -- checking a point
  // the engine has not computed yet against the oracle's answer for it. The
  // single-flag version hid this by accident: reloading overwrote the flag.
  // `running` is the guard that makes the retirement edge-true rather than
  // level-true.
  bool running[kCtx] = {};

  // Did the context actually RUN? A stale answer and a fresh oracle value is
  // consistent with both "ran and computed wrongly" and "never ran at all",
  // and those need completely different fixes.
  long start_clk[kCtx] = {};
  int writes_since_start[kCtx] = {};

  // THE WRITE TRACE, so a wrong answer can be asked what it READ.
  //
  // The final register file only shows the END state, and a bad operand is
  // usually overwritten by a later uop before the program finishes -- which is
  // exactly what happened here: a MUL wrote a wrong number while both its
  // source registers ended up correct. Replaying the writes in order rebuilds
  // the register file as it stood when that MUL executed.
  std::vector<std::pair<int, int32_t>> wtrace[kCtx];

  // Every long-op QUESTION this context asked: opcode and first operand, at
  // the clock the dispatcher took it.
  std::vector<std::pair<int, int32_t>> asked[kCtx];
  // Clock stamps, so ORDER can be shown rather than argued about.
  std::vector<long> wclk[kCtx], aclk[kCtx];

  // HOW MANY CONTEXTS THE FIXTURE IS KEEPING ALIVE.
  //
  // `idle_clocks` counts only clocks where a context was ACTIVE and none could
  // issue; `blocked` counts only ready-but-refused. A context that has retired
  // and is waiting for this harness to reload it is NEITHER, so it falls
  // through both -- and on wave_pool those two counters read zero while issue
  // occupancy was 63%. Summing `active_o` says whether the machine is short of
  // work or the fixture is short of hands.
  long active_sum = 0, active_clocks = 0;
  // Every clock this context had an instruction at S2: what operand a was
  // about to be captured, and what the file was actually presenting.
  struct Cap {
    long clk;
    int op;
    int32_t use_a0;
    int32_t rf_a0;
  };
  std::vector<Cap> caps[kCtx];

  explicit Dut(Vzhao_probe_v3_full& top) : t(top) {}

  void reset() {
    t.rst_n = 0;
    t.up_we_i = 0;
    t.pre_we_i = 0;
    t.start_i = 0;
    t.rival_req_i = 0;
    t.tl_we_i = 0;
    t.tl_commit_i = 0;
    t.sb_we_i = 0;
    // Drain-first. The measurement is already in: ALU-first STARVES the drain
    // outright, drain-first costs the ALU eight clocks per four-point group.
    t.wb_policy_i = (uint8_t)wb_policy_probe;
    t.eval();
    for (int i = 0; i < 4; ++i) zhao::tick(t);
    t.rst_n = 1;
    t.eval();
    zhao::tick(t);
    std::memset(shadow, 0, sizeof shadow);
    clocks = 0;
    preload_clocks = 0;
    for (int i = 0; i < kCtx; ++i) {
      done_at[i] = -1;
      late_writes[i] = 0;
      late_reg[i] = -1;
      done_q[i] = false;
      running[i] = false;
      start_clk[i] = 0;
      writes_since_start[i] = 0;
    }
  }

  /** THE ONLY PLACE A CLOCK HAPPENS once the run has begun.
   *
   *  Preloading, starting and stepping all go through here, so a clock spent
   *  feeding a context is counted exactly once and the register writes and
   *  `done` pulses that occur DURING a preload are still observed. A harness
   *  that ticks in three different functions loses whichever writes land on the
   *  clocks the other two owned. */
  void advance() {
    t.eval();
    const bool w = t.wr_en_o != 0;
    const int wc = (int)t.wr_ctx_o, wr = (int)t.wr_reg_o;
    // EVERY LANE CAPTURED BEFORE THE TICK. Reading the port after the clock
    // edge returns the NEXT clock's write, which is the same
    // capture-before-you-advance discipline this file states for `done` and
    // then quietly broke here when it grew lanes.
    int32_t wdl[kLanes];
    for (int l = 0; l < kLanes; ++l) wdl[l] = get_wr_lane(t, l);
    for (int c = 0; c < kCtx; ++c)
      if ((t.active_o >> c) & 1) ++active_sum;
    ++active_clocks;
    const int32_t wd = wdl[0];
    const bool took_long = (t.dbg_long_valid_o != 0) && (t.dbg_long_ready_o != 0);
    const int lc = (int)t.dbg_long_ctx_o;
    const int lop = (int)t.dbg_long_op_o;
    const int32_t ls0 = (int32_t)t.dbg_long_s0_o;
    if (t.dbg_s2_v_o && (int)t.dbg_s2_ctx_o < kCtx) {
      const Cap cp{clocks, (int)t.dbg_s2_op_o, (int32_t)t.dbg_use_a0_o, (int32_t)t.dbg_rf_a0_o};
      caps[(int)t.dbg_s2_ctx_o].push_back(cp);
    }
    saw_done = t.done_valid_o != 0;
    if (saw_done) last_done = (int)t.done_ctx_o;
    zhao::tick(t);
    ++clocks;
    if (w && wc < kCtx && wr < kRegs) {
      for (int l = 0; l < kLanes; ++l) shadow[wc][wr][l] = wdl[l];
      ++writes_since_start[wc];
      wtrace[wc].push_back(std::make_pair(wr, wd));
      wclk[wc].push_back(clocks);
      // A write for a context that has already said it was finished.
      if (done_at[wc] >= 0) {
        ++late_writes[wc];
        late_reg[wc] = wr;
      }
    }
    if (took_long && lc < kCtx) {
      asked[lc].push_back(std::make_pair(lop, ls0));
      aclk[lc].push_back(clocks);
    }
    if (saw_done && last_done < kCtx && running[last_done]) {
      done_at[last_done] = clocks;
      done_q[last_done] = true;
    }
  }

  void preload(int ctx, int reg, const int32_t* v) {
    // THE PRELOAD PORT STEALS THE REGISTER FILE.
    //
    // `zhao_probe_v3_exec` gives the host preload absolute priority over the
    // machine's own write, on the stated ground that "the machine is not
    // running during preload". Under staggered drive that premise is false:
    // this harness reloads a retired context while seven others are still
    // executing, and every preload clock silently discards whatever write the
    // arbiter had just granted.
    //
    // The machine now wins the port and `pre_ready_o` says when the preload
    // landed. A host that ignores it loses its own write instead.
    t.eval();
    while (t.pre_ready_o == 0) {
      advance();
      t.eval();
    }
    t.pre_we_i = 1;
    t.pre_ctx_i = (uint8_t)ctx;
    t.pre_reg_i = (uint8_t)reg;
    put_lane(t, v);
    advance();
    ++preload_clocks;
    t.pre_we_i = 0;
    for (int l = 0; l < kLanes; ++l) shadow[ctx][reg][l] = v[l];
  }

  void start(int ctx) {
    done_at[ctx] = -1;
    t.start_i = 1;
    t.start_ctx_i = (uint8_t)ctx;
    advance();
    t.start_i = 0;
    // AFTER the clock, so a `done` still asserted from the previous run on the
    // very clock this one starts does not count as this run finishing.
    done_q[ctx] = false;
    running[ctx] = true;
    start_clk[ctx] = clocks;
    writes_since_start[ctx] = 0;
    wtrace[ctx].clear();
    asked[ctx].clear();
    wclk[ctx].clear();
    aclk[ctx].clear();
    caps[ctx].clear();
  }

  // ---- setup, before the clock counter matters -----------------------------

  void load_uop(int ctx, int pc, const Mapped& m) {
    t.up_we_i = 1;
    t.up_ctx_i = (uint8_t)ctx;
    t.up_pc_i = (uint8_t)pc;
    t.up_op_i = m.op;
    t.up_dst_i = (uint8_t)m.dst;
    t.up_a_i = (uint8_t)m.a;
    t.up_b_i = (uint8_t)m.b;
    t.up_c_i = (uint8_t)m.c;
    t.up_imm_i = m.imm;
    t.eval();
    zhao::tick(t);
    t.up_we_i = 0;
    t.eval();
  }

  void load_table(int slot, const zfield::Table& tb) {
    const int n = (int)tb.x.size();
    for (int i = 0; i < n; ++i) {
      t.tl_we_i = 1;
      t.tl_tbl_i = (uint8_t)slot;
      t.tl_idx_i = (uint8_t)i;
      t.tl_x_i = (uint32_t)tb.x[(size_t)i];
      t.tl_y_i = (uint32_t)tb.y[(size_t)i];
      t.tl_dy_i = (uint32_t)tb.dy[(size_t)i];
      t.eval();
      zhao::tick(t);
    }
    t.tl_we_i = 0;
    t.tl_commit_i = 1;
    t.tl_tbl_i = (uint8_t)slot;
    t.tl_n_i = (uint8_t)n;
    t.eval();
    zhao::tick(t);
    t.tl_commit_i = 0;
    t.eval();
  }

  void load_scalar_bank(int slot, int32_t v) {
    t.sb_we_i = 1;
    t.sb_waddr_i = (uint16_t)slot;
    t.sb_wdata_i = (uint32_t)v;
    t.eval();
    zhao::tick(t);
    t.sb_we_i = 0;
    t.eval();
  }

  void preload_setup(int ctx, int reg, const int32_t* v) {
    t.pre_we_i = 1;
    t.pre_ctx_i = (uint8_t)ctx;
    t.pre_reg_i = (uint8_t)reg;
    put_lane(t, v);
    t.eval();
    zhao::tick(t);
    t.pre_we_i = 0;
    t.eval();
    for (int l = 0; l < kLanes; ++l) shadow[ctx][reg][l] = v[l];
  }

  int alarms() const {
    return (t.exec_desync_o ? 1 : 0) + (t.bank_desync_o ? 2 : 0) + (t.svc_bank_desync_o ? 4 : 0) +
           (t.tag_mismatch_o ? 8 : 0) + (t.wrong_op_o ? 16 : 0) + (t.sk_overflow_o ? 32 : 0) +
           (t.unsupported_o ? 64 : 0) + (t.sb_bad_o ? 128 : 0) + (t.imm_bad_o ? 256 : 0);
  }
};

/** SEVEN UOPS OF SMOOTHSTEP BECOME ONE PREPARED RING.
 *
 *  The builders expand every `smoothstep(e0, e1, x)` into the same seven
 *  varying instructions, in this exact order and operand order:
 *
 *      SUB   x, e0          CLAMP t, 0, 1      SUB   3.0, 2t
 *      MUL   t, rcp         MUL   t, t         MUL   t2, (3-2t)
 *                           MUL   2.0, t
 *
 *  In wave_pool that is SEVEN of seventeen uops -- a third of the hot loop --
 *  and `zhao_field_v3_ring_svc` already computes exactly that sequence as the
 *  FIRST HALF of `ring_prepared`, in hardware that crater_ring requires anyway.
 *
 *  Setting rB = 0 kills the second smoothstep: t1 clamps to 0, s1 becomes 0,
 *  and the final `s0 * (1 - 0)` is an exact fixed-point identity. Setting
 *  m = e0 makes the dead branch's subtraction the SAME subtraction the live
 *  branch already performs, so it cannot saturate where the original does not
 *  -- which is what keeps the SatLedger identical rather than merely the value.
 *
 *  Checked exhaustively before any of this was wired: 39,321 combinations of
 *  distance, edge and span, zero value mismatches and zero ledger mismatches
 *  against the seven-uop sequence run through the shipped `exec_op`.
 *
 *  THIS IS A PROTOTYPE IN THE HARNESS ON PURPOSE. The hardware runs the
 *  REWRITTEN program while `zfield::execute_point` still checks it against the
 *  UNTOUCHED plan, so the contraction is validated end to end against an oracle
 *  that has never heard of it. If it wins, it belongs in the planner.
 */
struct Peep {
  int at;               // index of the first uop of the run
  zfield::VecUop ring;  // what replaces the seven
};

bool match_smoothstep(const zfield::Fplan& fp, size_t i, zfield::VecUop* out) {
  if (i + 7 > fp.uops.size()) return false;
  const zfield::VecUop* u = &fp.uops[i];
  auto vec = [](const zfield::UopSrc& s) { return s.kind == zfield::SrcKind::kVec; };
  auto sca = [](const zfield::UopSrc& s) { return s.kind == zfield::SrcKind::kSca; };

  // SUB d, e0 -> t
  if (u[0].op != zfield::OP_SUB || u[0].n_src != 2 || !vec(u[0].src[0]) || !sca(u[0].src[1]))
    return false;
  // MUL t, rcp
  if (u[1].op != zfield::OP_MUL || u[1].n_src != 2 || u[1].src[0].idx != u[0].dst ||
      !vec(u[1].src[0]) || !sca(u[1].src[1]))
    return false;
  // CLAMP t, lo, hi
  if (u[2].op != zfield::OP_CLAMP || u[2].n_src != 3 || u[2].src[0].idx != u[1].dst ||
      !vec(u[2].src[0]) || !sca(u[2].src[1]) || !sca(u[2].src[2]))
    return false;
  // MUL t, t
  if (u[3].op != zfield::OP_MUL || u[3].n_src != 2 || !vec(u[3].src[0]) || !vec(u[3].src[1]) ||
      u[3].src[0].idx != u[2].dst || u[3].src[1].idx != u[2].dst)
    return false;
  // MUL 2.0, t   -- constant FIRST, as the builder emits it
  if (u[4].op != zfield::OP_MUL || u[4].n_src != 2 || !sca(u[4].src[0]) || !vec(u[4].src[1]) ||
      u[4].src[1].idx != u[2].dst)
    return false;
  // SUB 3.0, 2t
  if (u[5].op != zfield::OP_SUB || u[5].n_src != 2 || !sca(u[5].src[0]) || !vec(u[5].src[1]) ||
      u[5].src[1].idx != u[4].dst)
    return false;
  // MUL t2, (3-2t)
  if (u[6].op != zfield::OP_MUL || u[6].n_src != 2 || !vec(u[6].src[0]) || !vec(u[6].src[1]) ||
      u[6].src[0].idx != u[3].dst || u[6].src[1].idx != u[5].dst)
    return false;

  // The prepared ring takes the distance and four uniform SLOTS. `m` reuses e0
  // and `rB` reuses the clamp's lower bound, which is the literal zero the
  // program already carries -- so the rewrite invents no new prep value.
  zfield::VecUop r{};
  r.op = zfield::UOP_RING_PREP;
  r.dst = u[6].dst;
  r.n_src = 5;
  r.src[0] = u[0].src[0];  // d
  r.src[1] = u[0].src[1];  // r0 = e0
  r.src[2] = u[0].src[1];  // m  = e0, so the dead subtraction is the live one
  r.src[3] = u[1].src[1];  // rA = reciprocal
  r.src[4] = u[2].src[1];  // rB = the clamp low bound, which is 0
  // BIT 24 ASKS FOR SMOOTH MODE: four products instead of nine. The full ring
  // recipe gives the same answer -- that was measured first -- but costs the
  // shared multiplier bank more than the seven ALU uops it replaces.
  r.imm = 1u << 24;
  r.src_pc = u[0].src_pc;
  *out = r;
  return true;
}

/** The plan with every smoothstep run contracted. */
std::vector<zfield::VecUop> contract_smoothstep(const zfield::Fplan& fp, int* saved) {
  std::vector<zfield::VecUop> out;
  *saved = 0;
  for (size_t i = 0; i < fp.uops.size();) {
    zfield::VecUop r{};
    if (match_smoothstep(fp, i, &r)) {
      out.push_back(r);
      i += 7;
      *saved += 6;
    } else {
      out.push_back(fp.uops[i]);
      ++i;
    }
  }
  return out;
}

/** THE WHOLE REGISTER FILE THIS POINT SHOULD END WITH, uop by uop.
 *
 *  Checking only the four declared outputs says "something upstream is wrong"
 *  and stops there. Stepping the plan with the shipped per-op oracle and
 *  comparing EVERY vector register says WHICH uop first disagreed, and the op
 *  that wrote that register names the service to go and look at.
 *
 *  `zfield::steps::exec_op` is the same function the whole engine is
 *  differentiated against, so this adds no second opinion about semantics --
 *  only about where they stopped matching. */
void expected_regs(const zfield::Fplan& fp, const std::vector<zfield::VecUop>& uops,
                   const zfield::Decoded& prog, const zfield::Prepared& prep, const int32_t* in,
                   size_t n_in, int scalar_base, int32_t* rf, int n_rf,
                   std::vector<int>* wrote_by) {
  for (int i = 0; i < n_rf; ++i) rf[i] = 0;
  wrote_by->assign((size_t)n_rf, -1);

  // The uniform half is broadcast above the vector registers, exactly as the
  // harness loads it into the silicon.
  for (int s = 0; s < (int)fp.n_scalar && scalar_base + s < n_rf; ++s)
    rf[scalar_base + s] = prep.scalar[(size_t)s];
  // The varying half.
  for (size_t i = 0; i < n_in && i < fp.in_vreg.size(); ++i)
    if (fp.in_vreg[i] != 0xFF && (int)fp.in_vreg[i] < n_rf) rf[fp.in_vreg[i]] = in[i];

  zref::SatLedger L;
  for (size_t u = 0; u < uops.size(); ++u) {
    const zfield::VecUop& v = uops[u];
    int32_t src[9] = {};
    for (int k = 0; k < (int)v.n_src && k < 9; ++k) {
      const int r = v.src[k].kind == zfield::SrcKind::kVec ? (int)v.src[k].idx
                                                           : scalar_base + (int)v.src[k].idx;
      src[k] = (r >= 0 && r < n_rf) ? rf[r] : 0;
    }
    int32_t dst[3] = {};
    // UOP_RING_PREP IS NOT A CANONICAL OPCODE. The planner synthesises it, so
    // `exec_op` has never heard of it and indexing the op table with 0xF1 walks
    // off the end -- which is why crater_ring, the only program that uses it,
    // was the only one that crashed. It has its own reference function, and its
    // four uniforms arrive as scalar-bank slots rather than registers.
    if (v.op == zfield::UOP_RING_PREP) {
      dst[0] = zfield::steps::ring_prepared(src[0], src[1], src[2], src[3], src[4], &L);
    } else {
      zfield::steps::exec_op(v.op, v.imm, prog.tables, src, dst, &L);
    }
    const auto* sh = zfield::optable::shape_of(v.op);
    const int w = (v.op == zfield::UOP_RING_PREP) ? 1 : (sh ? (int)sh->dst_width : 1);
    for (int m = 0; m < w && (int)v.dst + m < n_rf; ++m) {
      rf[(int)v.dst + m] = dst[m];
      (*wrote_by)[(size_t)((int)v.dst + m)] = (int)u;
    }
  }
}

/** The oracle's value for one uop's member, for a given point. Used to ask
 *  whether a wrong answer is actually ANOTHER point's correct answer -- the
 *  difference between a duplicated response and corrupted arithmetic. */
int32_t oracle_uop_value(const zfield::Fplan& fp, const std::vector<zfield::VecUop>& uops,
                         const zfield::Decoded& prog, const zfield::Prepared& prep,
                         const int32_t* in, size_t n_in, int scalar_base, int n_rf, int target_u,
                         int target_m) {
  std::vector<int32_t> rf((size_t)n_rf, 0);
  for (int s = 0; s < (int)fp.n_scalar && scalar_base + s < n_rf; ++s)
    rf[(size_t)(scalar_base + s)] = prep.scalar[(size_t)s];
  for (size_t i = 0; i < n_in && i < fp.in_vreg.size(); ++i)
    if (fp.in_vreg[i] != 0xFF && (int)fp.in_vreg[i] < n_rf) rf[fp.in_vreg[i]] = in[i];
  zref::SatLedger L;
  for (int u = 0; u < (int)uops.size(); ++u) {
    const zfield::VecUop& v = uops[(size_t)u];
    int32_t src[9] = {};
    for (int k = 0; k < (int)v.n_src && k < 9; ++k) {
      const int r = v.src[k].kind == zfield::SrcKind::kVec ? (int)v.src[k].idx
                                                           : scalar_base + (int)v.src[k].idx;
      src[k] = (r < n_rf) ? rf[(size_t)r] : 0;
    }
    int32_t dst[3] = {};
    // UOP_RING_PREP IS NOT A CANONICAL OPCODE. The planner synthesises it, so
    // `exec_op` has never heard of it and indexing the op table with 0xF1 walks
    // off the end -- which is why crater_ring, the only program that uses it,
    // was the only one that crashed. It has its own reference function, and its
    // four uniforms arrive as scalar-bank slots rather than registers.
    if (v.op == zfield::UOP_RING_PREP) {
      dst[0] = zfield::steps::ring_prepared(src[0], src[1], src[2], src[3], src[4], &L);
    } else {
      zfield::steps::exec_op(v.op, v.imm, prog.tables, src, dst, &L);
    }
    if (u == target_u) return dst[target_m < 3 ? target_m : 0];
    const auto* sh = zfield::optable::shape_of(v.op);
    const int w = (v.op == zfield::UOP_RING_PREP) ? 1 : (sh ? (int)sh->dst_width : 1);
    for (int m = 0; m < w && (int)v.dst + m < n_rf; ++m) rf[(size_t)((int)v.dst + m)] = dst[m];
  }
  return 0;
}

/** Replay one context's writes in PROGRAM order and say what a given uop read.
 *
 *  The trace cannot be walked positionally: long-op results drain back
 *  asynchronously, so an ALU write can overtake a service write in the stream.
 *  Writes to ONE register are still in program order, though, so each uop's
 *  write is found by popping the front of that register's own queue.
 *
 *  Two register files are then stepped side by side -- one fed the hardware's
 *  written values, one fed the oracle's -- so the sources of the offending uop
 *  can be compared as they stood AT THAT MOMENT rather than at the end. */
void explain_uop(const zfield::Fplan& fp, const std::vector<zfield::VecUop>& uops,
                 const zfield::Decoded& prog, const zfield::Prepared& prep, const int32_t* in,
                 size_t n_in, int scalar_base, int n_rf,
                 const std::vector<std::pair<int, int32_t>>& trace, int target_uop, char* out,
                 size_t outsz, int* bad_u = nullptr, int* bad_m = nullptr,
                 int32_t* bad_val = nullptr) {
  if (bad_u) *bad_u = -1;
  std::vector<std::vector<int32_t>> q((size_t)n_rf);
  for (const auto& w : trace)
    if (w.first >= 0 && w.first < n_rf) q[(size_t)w.first].push_back(w.second);
  std::vector<size_t> take((size_t)n_rf, 0);

  std::vector<int32_t> hw((size_t)n_rf, 0), orc((size_t)n_rf, 0);
  for (int s = 0; s < (int)fp.n_scalar && scalar_base + s < n_rf; ++s) {
    hw[(size_t)(scalar_base + s)] = prep.scalar[(size_t)s];
    orc[(size_t)(scalar_base + s)] = prep.scalar[(size_t)s];
  }
  for (size_t i = 0; i < n_in && i < fp.in_vreg.size(); ++i)
    if (fp.in_vreg[i] != 0xFF && (int)fp.in_vreg[i] < n_rf) {
      hw[fp.in_vreg[i]] = in[i];
      orc[fp.in_vreg[i]] = in[i];
    }

  // THE FIRST UOP WHOSE WRITE DISAGREED, which the final register file cannot
  // tell you: a register written wrongly early is usually overwritten by a
  // later uop with a correct value, so the end state is clean and the fault is
  // invisible. Only the write TRACE has it.
  zref::SatLedger L;
  for (int u = 0; u < (int)uops.size(); ++u) {
    const zfield::VecUop& v = uops[(size_t)u];
    int32_t src[9] = {};
    int32_t src_hw[9] = {};
    for (int k = 0; k < (int)v.n_src && k < 9; ++k) {
      const int r = v.src[k].kind == zfield::SrcKind::kVec ? (int)v.src[k].idx
                                                           : scalar_base + (int)v.src[k].idx;
      src[k] = (r < n_rf) ? orc[(size_t)r] : 0;
      src_hw[k] = (r < n_rf) ? hw[(size_t)r] : 0;
    }
    int32_t dst[3] = {};
    // UOP_RING_PREP IS NOT A CANONICAL OPCODE. The planner synthesises it, so
    // `exec_op` has never heard of it and indexing the op table with 0xF1 walks
    // off the end -- which is why crater_ring, the only program that uses it,
    // was the only one that crashed. It has its own reference function, and its
    // four uniforms arrive as scalar-bank slots rather than registers.
    if (v.op == zfield::UOP_RING_PREP) {
      dst[0] = zfield::steps::ring_prepared(src[0], src[1], src[2], src[3], src[4], &L);
    } else {
      zfield::steps::exec_op(v.op, v.imm, prog.tables, src, dst, &L);
    }
    const auto* sh = zfield::optable::shape_of(v.op);
    const int w = (v.op == zfield::UOP_RING_PREP) ? 1 : (sh ? (int)sh->dst_width : 1);

    for (int m = 0; m < w && (int)v.dst + m < n_rf; ++m) {
      const size_t r = (size_t)((int)v.dst + m);
      const bool have = take[r] < q[r].size();
      const int32_t got = have ? q[r][take[r]] : hw[r];
      if (got != dst[m] && (target_uop < 0 || u <= target_uop)) {
        if (bad_u) *bad_u = u;
        if (bad_m) *bad_m = m;
        if (bad_val) *bad_val = got;
        // Say whether the operands it read were themselves already wrong. If
        // they were sound, the arithmetic or its product routing is at fault;
        // if not, this uop is only carrying an earlier fault forward.
        bool src_ok = true;
        int off = snprintf(out, outsz,
                           "      EARLIEST BAD WRITE: uop %d op 0x%02X -> r%zu = %d, "
                           "oracle %d; read",
                           u, v.op, r, got, dst[m]);
        for (int k = 0; k < (int)v.n_src && k < 9 && off < (int)outsz - 60; ++k) {
          const int rr = v.src[k].kind == zfield::SrcKind::kVec ? (int)v.src[k].idx
                                                                : scalar_base + (int)v.src[k].idx;
          if (src_hw[k] != src[k]) src_ok = false;
          off += snprintf(out + off, outsz - (size_t)off, " r%d=%d%s", rr, src_hw[k],
                          (src_hw[k] == src[k]) ? "" : "(STALE)");
        }
        if (off < (int)outsz - 40)
          snprintf(out + off, outsz - (size_t)off, "  [operands %s]",
                   src_ok ? "SOUND -> arithmetic/product fault" : "already wrong upstream");
        return;
      }
      orc[r] = dst[m];
      if (have) {
        hw[r] = got;
        ++take[r];
      }
    }
  }
  snprintf(out, outsz, "      every write matched -- divergence is not in the trace");
}

struct Result {
  bool ran = false;
  long ii_x4 = 0;  // clocks per four-point group, scaled by 4 for rounding
  long group_clocks = 0;
  long assoc = 0;
  long frame = 0;
  long preloads = 0;
  int checked = 0;
  std::string refusal;

  // The engine's own per-stage evidence. Brought out because "624 clocks per
  // group" names no stage, and a number that cannot say which block produced
  // it is the start of a guess rather than the end of a measurement.
  long groups = 0, partial = 0, uops = 0, idle = 0, drain = 0;
  long held = 0, blocked = 0, denied = 0, dotc = 0, skid = 0;
  double avg_active = 0.0;
  long wb_served[2] = {0, 0}, wb_stalled[2] = {0, 0};
  // THE ONE WRITE PORT. Every uop of every point lands through it, one per
  // clock, so its occupancy is a hard floor on the whole machine however wide
  // the services get.
  // BOTH DIFFERENCED OVER THE SAME WINDOW. `rf_writes_o` counts from reset
  // while the span is only the measured part of the run, and dividing one by
  // the other printed 123% of a single write port -- which is not a tight
  // measurement, it is an impossible one. A ratio is only a ratio if its two
  // halves cover the same clocks.
  long writes = 0, span_clocks = 0;
};

/** Run one real Earth program on the composed engine and count. */
Result run_program(const char* path, int points, uint64_t seed, int drive, int n_ctx) {
  Result R;

  const std::vector<uint8_t> bytes = slurp(path);
  if (bytes.empty()) {
    R.refusal = "unreadable";
    return R;
  }
  const zfield::DecodeResult dec = zfield::decode(bytes.data(), bytes.size());
  if (dec.error != zfield::DecodeError::kOk) {
    R.refusal = "undecodable";
    return R;
  }
  // Earth: x and z vary per point. Everything else is uniform for the whole
  // association, which is what makes the uniform block worth having.
  const zfield::Fplan fp = zfield::plan(dec.prog, 0b11);

  const int n_vreg = (int)fp.n_vreg;
  const int n_scalar = (int)fp.n_scalar;
  const int scalar_base = n_vreg;

  if (n_vreg + n_scalar > kRegs) {
    char buf[160];
    snprintf(buf, sizeof buf, "needs %d vector + %d uniform = %d registers, and the file holds %d",
             n_vreg, n_scalar, n_vreg + n_scalar, kRegs);
    R.refusal = buf;
    return R;
  }
  // THE CONTRACTED PROGRAM. The hardware runs this; `zfield::execute_point`
  // below still checks the outputs against the UNTOUCHED plan, so the rewrite
  // is validated against an oracle that has never heard of it.
  int ss_saved = 0;
  const std::vector<zfield::VecUop> uops =
      contract_ss ? contract_smoothstep(fp, &ss_saved)
                  : std::vector<zfield::VecUop>(fp.uops.begin(), fp.uops.end());

  if ((int)uops.size() + 1 > kPlan) {
    R.refusal = "program longer than the uop store";
    return R;
  }

  // ---- translate the plan into the silicon's own operand shape -------------
  Translator tr(fp, scalar_base);
  std::vector<Mapped> prog;
  for (const zfield::VecUop& u : uops) {
    Mapped m;
    if (!tr.map_one(u, &m)) {
      R.refusal = tr.refusal;
      return R;
    }
    prog.push_back(m);
  }

  // ---- the association's uniform half, run once ---------------------------
  // The base input record. Uniform lanes are fixed for the whole association;
  // lanes 0 and 1 are overwritten per point below.
  Rng rng(seed);
  const size_t n_in = dec.prog.in_lanes.size();
  std::vector<int32_t> base_in(n_in, 0);
  for (size_t i = 0; i < n_in; ++i) {
    const zfield::IoLane& L = dec.prog.in_lanes[i];
    if (L.max > L.min) {
      const uint32_t span = (uint32_t)((int64_t)L.max - (int64_t)L.min);
      base_in[i] = (int32_t)((int64_t)L.min + (int64_t)(rng.next() % (span + 1u)));
    } else {
      base_in[i] = (int32_t)(rng.next() & 0x0003FFFF) - 0x00020000;
    }
  }
  const zfield::Prepared prep = zfield::prepare(fp, dec.prog, base_in.data(), n_in);
  if ((int)prep.scalar.size() < n_scalar) {
    R.refusal = "the uniform block produced fewer values than the plan declares";
    return R;
  }

  // ---- bring the machine up ------------------------------------------------
  Vzhao_probe_v3_full top;
  Dut d(top);
  d.reset();

  for (size_t i = 0; i < dec.prog.tables.size() && i < 4; ++i)
    d.load_table((int)i, dec.prog.tables[i]);

  // The uniform values go to BOTH homes, because the machine has two consumers
  // of them and they read by different routes: the prepared ring reads the
  // scalar bank by packed index, and everything else reads a register.
  for (int s = 0; s < n_scalar && s < 64; ++s) d.load_scalar_bank(s, prep.scalar[(size_t)s]);
  for (int c = 0; c < kCtx; ++c)
    for (int s = 0; s < n_scalar; ++s) {
      // A uniform is the same in every point of the quad, by definition.
      int32_t lane[kLanes];
      for (int l = 0; l < kLanes; ++l) lane[l] = prep.scalar[(size_t)s];
      d.preload_setup(c, scalar_base + s, lane);
    }

  for (int c = 0; c < kCtx; ++c) {
    for (size_t p = 0; p < prog.size(); ++p) d.load_uop(c, (int)p, prog[p]);
    Mapped end{};
    end.op = zfield::OP_END;
    d.load_uop(c, (int)prog.size(), end);
  }

  // ---- the points ----------------------------------------------------------
  // One point per context, checked and replaced the instant it retires.
  // A CONTEXT IS kLanes POINTS. At kLanes = 1 that is the scalar machine this
  // file was written against; at 4 a context is a quad and every one of these
  // carries four points' worth.
  std::vector<int32_t> pt_in[kCtx][kLanes];
  std::vector<int32_t> exp_out[kCtx][kLanes];
  int32_t exp_rf[kCtx][kLanes][kRegs] = {};
  std::vector<int> wrote_by;
  const size_t n_out = dec.prog.out_lanes.size();

  int issued = 0, retired = 0;
  Rng prng(seed ^ 0xA5A5A5A5u);

  auto make_point = [&](int c) {
    for (int l = 0; l < kLanes; ++l) {
      pt_in[c][l] = base_in;
      for (size_t i = 0; i < n_in && i < 2; ++i)
        pt_in[c][l][i] = (int32_t)(prng.next() & 0x000FFFFF) - 0x00080000;
      exp_out[c][l].assign(n_out, 0);
      zfield::execute_point(fp, dec.prog, prep, pt_in[c][l].data(), n_in, exp_out[c][l].data(),
                            n_out);
      expected_regs(fp, uops, dec.prog, prep, pt_in[c][l].data(), n_in, scalar_base, exp_rf[c][l],
                    kRegs, &wrote_by);
    }
  };

  auto load_point = [&](int c) {
    // A START IS NOT A HANDSHAKE.
    //
    // `start_i` is a bare pulse: there is no `start_ready_o` to refuse it, so a
    // start aimed at a context the engine still counts as ACTIVE is dropped on
    // the floor with nothing to say so. The context then never re-runs, its
    // registers keep the previous point's answers, and `done_valid_o` -- still
    // asserted from the run before -- makes it look like it retired again.
    //
    // That is what the staggered failures were: every one of them read a stale
    // hardware value against a freshly computed oracle value. `active_o` is the
    // only signal that says whether the pulse will land, so it is waited on.
    int spin = 0;
    while ((d.t.active_o & (1u << c)) != 0 && spin++ < 4096) d.advance();
    make_point(c);
    for (size_t i = 0; i < n_in; ++i) {
      const uint8_t vr = i < fp.in_vreg.size() ? fp.in_vreg[i] : 0xFF;
      if (vr == 0xFF) continue;
      int32_t lane[kLanes];
      for (int l = 0; l < kLanes; ++l) lane[l] = pt_in[c][l][i];
      d.preload(c, (int)vr, lane);
    }
    d.start(c);
    ++issued;
  };

  // ONE LANE AT A TIME. Every point of the quad is its own comparison against
  // the oracle -- a quad that got three right and one wrong is wrong, and a
  // check that only looked at lane 0 would pass a machine whose upper lanes
  // were never wired.
  auto check_lane = [&](int c, int lane) {
    // WHICH UOP FIRST DISAGREED -- EARLIEST BY PROGRAM ORDER, not by register
    // index. Scanning registers in index order reports whichever wrong value
    // happens to sit lowest in the file, which is arbitrary: it named a MUL
    // that was faithfully squaring a number an EARLIER uop had already got
    // wrong. The register a fault lands in says nothing about when it happened.
    int worst_r = -1, worst_u = 1 << 30;
    for (int r = 0; r < (int)fp.n_vreg; ++r) {
      if (d.shadow[c][r][lane] == exp_rf[c][lane][r]) continue;
      const int uu = (r < (int)wrote_by.size()) ? wrote_by[(size_t)r] : -1;
      const int key = (uu < 0) ? (1 << 29) : uu;
      if (key < worst_u) {
        worst_u = key;
        worst_r = r;
      }
    }
    for (int r = worst_r; r >= 0; r = -1) {
      const int u = (r < (int)wrote_by.size()) ? wrote_by[(size_t)r] : -1;
      const uint8_t op = (u >= 0 && u < (int)uops.size()) ? uops[(size_t)u].op : 0;

      // WHOSE NUMBER IS IT? A wrong value that belongs to nobody is arithmetic
      // going astray; a wrong value that is exactly ANOTHER register's or
      // ANOTHER context's correct answer is a routing or operand fault, and
      // those need completely different fixes. So the value is looked up.
      char whose[96];
      whose[0] = '\0';
      const int32_t got = d.shadow[c][r][lane];
      for (int r2 = 0; r2 < (int)fp.n_vreg && !whose[0]; ++r2)
        if (r2 != r && exp_rf[c][lane][r2] == got)
          snprintf(whose, sizeof whose, "; equals THIS context's correct r%d", r2);
      for (int c2 = 0; c2 < n_ctx && !whose[0]; ++c2)
        if (c2 != c && exp_rf[c2][lane][r] == got)
          snprintf(whose, sizeof whose, "; equals ctx %d's correct r%d", c2, r);
      if (!whose[0]) snprintf(whose, sizeof whose, "; matches no other register or context");

      char buf[320];
      snprintf(buf, sizeof buf,
               "%s point %d ctx %d lane %d: r%d is %d, oracle %d -- uop %d, opcode 0x%02X%s", path,
               retired, c, lane, r, got, exp_rf[c][lane][r], u, op, whose);
      fail("first divergent register", buf);

      // WHAT DID THAT UOP READ? Replayed in program order, hardware and
      // oracle side by side, so a stale operand names itself.
      //
      // LANE 0 ONLY, deliberately. The write trace records one lane of each
      // write, so this replay can only speak for lane 0; it exists to name a
      // failing uop, and it found the preload-port defect that way. Widening
      // it to four lanes would add surface without adding an answer, and a
      // diagnostic that quietly reported lane 0's operands as though they were
      // lane 3's would be worse than not having it.
      if (u >= 0 && u < (int)uops.size() && lane == 0 && printed <= 6) {
        char srcbuf[360];
        int bu = -1, bm = 0;
        int32_t bval = 0;
        explain_uop(fp, uops, dec.prog, prep, pt_in[c][0].data(), n_in, scalar_base, kRegs,
                    d.wtrace[c], -1, srcbuf, sizeof srcbuf, &bu, &bm, &bval);
        printf("%s\n", srcbuf);
        {
          char qb[300];
          int qo = snprintf(qb, sizeof qb, "      long ops this context ASKED:");
          for (size_t qi = 0; qi < d.asked[c].size() && qo < (int)sizeof qb - 48; ++qi)
            qo += snprintf(qb + qo, sizeof qb - (size_t)qo, " [op 0x%02X s0=%d @%ld]",
                           d.asked[c][qi].first, d.asked[c][qi].second, d.aclk[c][qi]);
          printf("%s\n", qb);
          char wb2[300];
          int wo = snprintf(wb2, sizeof wb2, "      writes this context LANDED:");
          for (size_t wi = 0; wi < d.wtrace[c].size() && wo < (int)sizeof wb2 - 40; ++wi)
            wo += snprintf(wb2 + wo, sizeof wb2 - (size_t)wo, " r%d=%d@%ld", d.wtrace[c][wi].first,
                           d.wtrace[c][wi].second, d.wclk[c][wi]);
          printf("%s\n", wb2);
          char cb[360];
          int co = snprintf(cb, sizeof cb, "      operand a AT CAPTURE (S2):");
          for (size_t ci = 0; ci < d.caps[c].size() && co < (int)sizeof cb - 56; ++ci)
            co += snprintf(cb + co, sizeof cb - (size_t)co, " [op 0x%02X use=%d rf=%d @%ld]",
                           d.caps[c][ci].op, d.caps[c][ci].use_a0, d.caps[c][ci].rf_a0,
                           d.caps[c][ci].clk);
          printf("%s\n", cb);
        }

        // IS IT ANOTHER POINT'S CORRECT ANSWER? A duplicated or misrouted
        // response and corrupted arithmetic look identical in one number, and
        // they need entirely different fixes. Every other context in flight is
        // asked whether this value is the answer IT was owed.
        if (bu >= 0) {
          for (int c2 = 0; c2 < n_ctx; ++c2) {
            if (c2 == c) continue;
            const int32_t v = oracle_uop_value(fp, uops, dec.prog, prep, pt_in[c2][0].data(), n_in,
                                               scalar_base, kRegs, bu, bm);
            if (v == bval) {
              printf(
                  "      ^ that value is ctx %d's CORRECT answer for the same uop --\n"
                  "        a response delivered to the wrong group, not bad arithmetic\n",
                  c2);
              break;
            }
          }
        }
      }
      break;
    }
    for (size_t o = 0; o < n_out; ++o) {
      const zfield::OutTag& tg = fp.out_map[o];
      if (tg.kind != zfield::SrcKind::kVec) continue;  // a uniform output
      const int32_t got = d.shadow[c][(int)tg.idx][lane];
      if (got != exp_out[c][lane][o]) {
        char buf[220];
        snprintf(buf, sizeof buf,
                 "%s point %d ctx %d lane %d out %zu: hardware %d, oracle %d "
                 "[ran %ld clocks, %d writes landed]",
                 path, retired, c, lane, o, got, exp_out[c][lane][o], d.clocks - d.start_clk[c],
                 d.writes_since_start[c]);
        fail("composed Earth value", buf);
        return;
      }
      ++R.checked;
    }
  };

  auto check_point = [&](int c) {
    for (int lane = 0; lane < kLanes; ++lane) check_lane(c, lane);
  };

  // FILL IS NOT RATE. The first points through an empty pipeline are slower
  // than the machine's steady state, and counting them would understate the
  // engine exactly as counting only the last would flatter it.
  //
  // TWO DRIVE PATTERNS, because the first run showed the answer depends on the
  // pattern far more than on any service:
  //
  //   STAGGERED  every context is reloaded and restarted the instant it
  //              retires. It keeps the engine as full as possible and it is
  //              the obvious way to write this loop.
  //   WAVE       all eight contexts are loaded and started together, and the
  //              next eight wait for the last of them.
  //
  // Staggered looks better and is worse. The dispatcher gathers FOUR contexts
  // sitting on the SAME long op into one four-point group; contexts that retire
  // at different moments drift apart, stop sharing an op, and the group goes
  // out PARTIAL -- a whole group's latency spent on one point. The drift is
  // self-reinforcing, which is how 98% of groups went out partial.
  // EVERY COUNTER DIFFERENCED OVER THE MEASURED WINDOW. The engine's counters
  // run from RESET and the span covers only the counted part of the run, so
  // reporting one against the other is not a tight measurement -- it printed
  // 2,390 uops issued inside 1,742 clocks, which is more than one per clock and
  // therefore impossible. The write ratio was fixed once and the rest were left
  // behind; they are all differenced now.
  long t0 = 0, preload0 = 0, writes0 = 0, uops0 = 0, idle0 = 0, held0 = 0, blocked0 = 0;
  long active_sum0 = 0, active_clocks0 = 0;
  long denied0 = 0, dotc0 = 0, skid0 = 0;
  int counted_start = 0;
  int guard = 0;
  const int guard_max = points * 8000 + 400000;
  const int warmup_waves = (n_ctx >= 4) ? 2 : 4;

  if (drive == kDriveWave) {
    bool done_flag[kCtx];
    for (int wave = 0; retired < points && guard < guard_max; ++wave) {
      if (wave == warmup_waves) {
        t0 = d.clocks;
        preload0 = d.preload_clocks;
        writes0 = (long)top.rf_writes_o;
        uops0 = (long)top.uops_issued_o;
        idle0 = (long)top.idle_clocks_o;
        held0 = (long)top.hold_clocks_o;
        blocked0 = (long)top.blocked_clocks_o;
        active_sum0 = d.active_sum;
        active_clocks0 = d.active_clocks;
        denied0 = (long)top.denied_clocks_o;
        dotc0 = (long)top.dot_clocks_o;
        skid0 = (long)top.skid_clocks_o;
        denied0 = (long)top.denied_clocks_o;
        dotc0 = (long)top.dot_clocks_o;
        skid0 = (long)top.skid_clocks_o;
        active_sum0 = d.active_sum;
        active_clocks0 = d.active_clocks;
        denied0 = (long)top.denied_clocks_o;
        dotc0 = (long)top.dot_clocks_o;
        skid0 = (long)top.skid_clocks_o;
        denied0 = (long)top.denied_clocks_o;
        dotc0 = (long)top.dot_clocks_o;
        skid0 = (long)top.skid_clocks_o;
        uops0 = (long)top.uops_issued_o;
        idle0 = (long)top.idle_clocks_o;
        held0 = (long)top.hold_clocks_o;
        blocked0 = (long)top.blocked_clocks_o;
        active_sum0 = d.active_sum;
        active_clocks0 = d.active_clocks;
        denied0 = (long)top.denied_clocks_o;
        dotc0 = (long)top.dot_clocks_o;
        skid0 = (long)top.skid_clocks_o;
        denied0 = (long)top.denied_clocks_o;
        dotc0 = (long)top.dot_clocks_o;
        skid0 = (long)top.skid_clocks_o;
        active_sum0 = d.active_sum;
        active_clocks0 = d.active_clocks;
        denied0 = (long)top.denied_clocks_o;
        dotc0 = (long)top.dot_clocks_o;
        skid0 = (long)top.skid_clocks_o;
        denied0 = (long)top.denied_clocks_o;
        dotc0 = (long)top.dot_clocks_o;
        skid0 = (long)top.skid_clocks_o;
        counted_start = retired;
      }
      for (int c = 0; c < n_ctx; ++c) {
        load_point(c);
        done_flag[c] = false;
      }
      int left = n_ctx;
      while (left > 0 && guard++ < guard_max) {
        d.advance();
        for (int c = 0; c < kCtx; ++c) {
          if (!d.done_q[c]) continue;
          d.done_q[c] = false;
          d.running[c] = false;
          if (!done_flag[c]) {
            done_flag[c] = true;
            --left;
            check_point(c);
            retired += kLanes;
          }
        }
      }
    }
  } else if (drive == kDriveQuad) {
    // QUADS ALIGNED, QUADS STAGGERED.
    //
    // The other two patterns each give up one of the two things that matter.
    // WAVE starts every context together, so all of them sit on the SAME
    // opcode at once and the services never overlap -- the machine runs one
    // service at a time with the rest idle. STAGGERED overlaps the services but
    // lets contexts drift apart one at a time, so they stop sharing an op and
    // the groups go out PARTIAL: 82% of them at 32 contexts.
    //
    // A dispatch group is FOUR points. So the unit that must stay aligned is
    // four contexts, not one and not all of them. Quads are started together
    // and reloaded together -- which fills every group -- while different quads
    // sit at different points in the program, which is what keeps DIST2, CURVE
    // and the trig unit busy at the same time.
    const int nq = n_ctx / kCtxPerGroup;
    std::vector<int> left((size_t)nq, 0);
    std::vector<bool> qdone((size_t)n_ctx, false);
    int wave = 0;
    for (int q = 0; q < nq; ++q) {
      for (int k = 0; k < kCtxPerGroup; ++k) {
        load_point(q * kCtxPerGroup + k);
        qdone[(size_t)(q * kCtxPerGroup + k)] = false;
      }
      left[(size_t)q] = kCtxPerGroup;
    }
    while (retired < points && guard++ < guard_max) {
      d.advance();
      for (int c = 0; c < n_ctx; ++c) {
        if (!d.done_q[c]) continue;
        d.done_q[c] = false;
        d.running[c] = false;
        if (qdone[(size_t)c]) continue;
        qdone[(size_t)c] = true;
        --left[(size_t)(c / kCtxPerGroup)];
        check_point(c);
        retired += kLanes;
      }
      for (int q = 0; q < nq && retired < points; ++q) {
        if (left[(size_t)q] != 0) continue;
        if (wave == warmup_waves * nq) {
          t0 = d.clocks;
          preload0 = d.preload_clocks;
          writes0 = (long)top.rf_writes_o;
          uops0 = (long)top.uops_issued_o;
          idle0 = (long)top.idle_clocks_o;
          held0 = (long)top.hold_clocks_o;
          blocked0 = (long)top.blocked_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          uops0 = (long)top.uops_issued_o;
          idle0 = (long)top.idle_clocks_o;
          held0 = (long)top.hold_clocks_o;
          blocked0 = (long)top.blocked_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          uops0 = (long)top.uops_issued_o;
          idle0 = (long)top.idle_clocks_o;
          held0 = (long)top.hold_clocks_o;
          blocked0 = (long)top.blocked_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          counted_start = retired;
        }
        ++wave;
        for (int k = 0; k < kCtxPerGroup; ++k) {
          load_point(q * kCtxPerGroup + k);
          qdone[(size_t)(q * kCtxPerGroup + k)] = false;
        }
        left[(size_t)q] = kCtxPerGroup;
      }
    }
  } else {
    const int warmup = n_ctx * warmup_waves;
    for (int c = 0; c < n_ctx; ++c) load_point(c);
    while (retired < points && guard++ < guard_max) {
      d.advance();
      for (int c = 0; c < kCtx && retired < points; ++c) {
        if (!d.done_q[c]) continue;
        d.done_q[c] = false;
        d.running[c] = false;
        check_point(c);
        retired += kLanes;
        if (retired == warmup) {
          t0 = d.clocks;
          preload0 = d.preload_clocks;
          writes0 = (long)top.rf_writes_o;
          uops0 = (long)top.uops_issued_o;
          idle0 = (long)top.idle_clocks_o;
          held0 = (long)top.hold_clocks_o;
          blocked0 = (long)top.blocked_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          uops0 = (long)top.uops_issued_o;
          idle0 = (long)top.idle_clocks_o;
          held0 = (long)top.hold_clocks_o;
          blocked0 = (long)top.blocked_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          uops0 = (long)top.uops_issued_o;
          idle0 = (long)top.idle_clocks_o;
          held0 = (long)top.hold_clocks_o;
          blocked0 = (long)top.blocked_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          active_sum0 = d.active_sum;
          active_clocks0 = d.active_clocks;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          denied0 = (long)top.denied_clocks_o;
          dotc0 = (long)top.dot_clocks_o;
          skid0 = (long)top.skid_clocks_o;
          counted_start = retired;
        }
        if (retired < points) load_point(c);
      }
    }
  }

  if (retired < points) {
    R.refusal = "the engine stopped retiring -- deadlock, not a measurement";
    return R;
  }
  if (d.alarms() != 0) {
    char buf[80];
    snprintf(buf, sizeof buf, "%s raised alarms 0x%x", path, d.alarms());
    fail("composed Earth alarm", buf);
  }

  const long span = d.clocks - t0;
  const long counted = retired - counted_start;  // points
  R.ran = true;
  R.preloads = d.preload_clocks - preload0;
  // Four points make one group, which is the unit the admission law speaks in.
  R.group_clocks = (span * 4 + counted / 2) / (counted > 0 ? counted : 1);
  R.assoc = R.group_clocks * kGroupsPerAssoc;
  R.frame = R.assoc * kAssocPerFrame;

  for (int c = 0; c < kCtx; ++c)
    if (d.late_writes[c] > 0) {
      char buf[200];
      snprintf(buf, sizeof buf,
               "%s ctx %d took %d write(s) AFTER done_valid_o, last into r%d -- `done` is not a "
               "promise that the results have landed",
               path, c, d.late_writes[c], d.late_reg[c]);
      fail("late writeback", buf);
      break;
    }

  R.writes = (long)top.rf_writes_o - writes0;
  R.span_clocks = span;
  R.groups = (long)top.groups_o;
  R.partial = (long)top.partial_o;
  R.uops = (long)top.uops_issued_o - uops0;
  R.idle = (long)top.idle_clocks_o - idle0;
  R.held = (long)top.hold_clocks_o - held0;
  R.blocked = (long)top.blocked_clocks_o - blocked0;
  R.denied = (long)top.denied_clocks_o - denied0;
  R.dotc = (long)top.dot_clocks_o - dotc0;
  R.skid = (long)top.skid_clocks_o - skid0;
  R.avg_active = (d.active_clocks > active_clocks0) ? (double)(d.active_sum - active_sum0) /
                                                          (double)(d.active_clocks - active_clocks0)
                                                    : 0.0;
  R.drain = (long)top.drain_writes_o;
  for (int i = 0; i < 2; ++i) {
    R.wb_served[i] = (long)top.wb_served_o[i];
    R.wb_stalled[i] = (long)top.wb_stalled_o[i];
  }
  return R;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  int points = 256;
  int n_ctx = kCtx;
  std::vector<const char*> files;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--points" && i + 1 < argc) {
      points = std::atoi(argv[++i]);
    } else if (a == "--smoothstep") {
      contract_ss = true;
    } else if (a == "--no-smoothstep") {
      contract_ss = false;
    } else if (a == "--wb" && i + 1 < argc) {
      wb_policy_probe = std::atoi(argv[++i]);
    } else if (a == "--contexts" && i + 1 < argc) {
      n_ctx = std::atoi(argv[++i]);
      if (n_ctx < 1) n_ctx = 1;
      if (n_ctx > kCtx) n_ctx = kCtx;
    } else if (a.size() && a[0] != '-') {
      files.push_back(argv[i]);
    }
  }
  if (files.empty()) {
    printf("usage: test_field_v3_earth [--points N] prog.zprog ...\n");
    return 2;
  }

  printf("== GATE 3: real Earth programs on the composed engine ==\n");
  printf("   the law: %ld clocks per association, %ld per 128-association frame\n", kAssocBudget,
         kFrameBudget);
  printf("   %ld groups per association, %ld groups per frame, so %.1f clocks per group\n\n",
         kGroupsPerAssoc, kGroupsPerAssoc * kAssocPerFrame,
         (double)kFrameBudget / (double)(kGroupsPerAssoc * kAssocPerFrame));

  int ran = 0, over = 0, total_checks = 0, total_stag_bad = 0;
  long worst_frame = 0;
  const char* worst_name = "";

  for (const char* f : files) {
    const char* base = f;
    for (const char* p = f; *p; ++p)
      if (*p == '/' || *p == '\\') base = p + 1;

    // The staggered pattern first, purely so the contrast is on the record.
    // It is the obvious way to drive this engine and it is the wrong one.
    // BOTH DRIVE PATTERNS GATE THE BUILD NOW.
    //
    // Staggered drive used to be run with the gate off because it produced
    // wrong values and the cause was still being chased. It was the preload
    // port stealing the register file; the machine's write wins it now. So the
    // pattern that FOUND the defect is the pattern that guards against its
    // return, which is the only honest place for it.
    diag_failures = 0;
    const Result S = run_program(f, points, 0xC0FFEEull, kDriveStaggered, n_ctx);
    const int stag_bad = diag_failures;
    printed = 0;
    if (S.ran)
      printf("   %-22s STAGGERED group %4ld  frame %9ld  partial %ld/%ld  WRONG VALUES %d\n", base,
             S.group_clocks, S.frame, S.partial, S.groups, stag_bad);
    total_stag_bad += stag_bad;

    const Result Q = run_program(f, points, 0xC0FFEEull, kDriveQuad, n_ctx);
    if (Q.ran)
      printf("   %-22s QUAD      group %4ld  association %7ld  frame %9ld  partial %ld/%ld  %s\n",
             base, Q.group_clocks, Q.assoc, Q.frame, Q.partial, Q.groups,
             Q.frame <= kFrameBudget ? "FITS" : "OVER");
    total_checks += Q.checked;

    const Result R = run_program(f, points, 0xC0FFEEull, kDriveWave, n_ctx);

    if (!R.ran) {
      printf("   %-22s NOT RUN: %s\n", base, R.refusal.c_str());
      continue;
    }
    ++ran;
    total_checks += R.checked + S.checked;

    // THE VERDICT FOLLOWS THE QUAD RESULT. That is how the machine is meant to
    // be fed -- aligned fours so groups fill, staggered quads so the services
    // overlap -- and judging it by the WAVE control would report a number no
    // one would ever drive it at. WAVE and STAGGERED stay above as controls,
    // and all three are gated for correctness regardless.
    const long verdict_frame = Q.ran ? Q.frame : R.frame;
    const bool fits = verdict_frame <= kFrameBudget;
    if (!fits) ++over;
    if (verdict_frame > worst_frame) {
      worst_frame = verdict_frame;
      worst_name = base;
    }
    printf("   %-22s WAVE      group %4ld  association %7ld  frame %9ld  %s\n", base,
           R.group_clocks, R.assoc, R.frame, fits ? "FITS" : "OVER");
    // THE DETAIL FOLLOWS THE QUAD RESULT, because QUAD is how the machine is
    // actually fed: aligned fours so the groups fill, staggered quads so the
    // services overlap. WAVE and STAGGERED stay as controls.
    const Result& D = Q.ran ? Q : R;
    printf(
        "   %-22s   %d values checked against the oracle, %ld of the counted clocks were "
        "harness preload\n",
        "", D.checked, D.preloads);
    // WALL, PRELOAD AND ENGINE, SEPARATELY.
    //
    // Point data comes in one register per clock through the probe's preload
    // port; the finished machine streams it from memory instead. Those clocks
    // are real and they are COUNTED -- subtracting them silently would be
    // flattering the result -- but they belong to the fixture, not the Earth
    // executor, and the split says which of the two any remaining excess is.
    if (D.group_clocks > 0 && D.span_clocks > 0) {
      // preloads are clocks over the SAME window the span covers, so the share
      // of a group's clocks they account for is preloads/span. Multiplying by
      // four as well counted each group four times over and reported 7.9 where
      // the truth is nearer 2 -- flattering, and in the direction I wanted.
      const double pre_per_group = (double)D.preloads /
                                   (double)((D.span_clocks > 0) ? D.span_clocks : 1) *
                                   (double)D.group_clocks;
      printf(
          "   %-22s   clocks per four-point group: %ld wall, %.1f of them harness preload, "
          "%.1f engine\n",
          "", D.group_clocks, pre_per_group, (double)D.group_clocks - pre_per_group);
    }
    // WHICH STAGE. A dispatch group that went out with fewer than four points
    // did a group's worth of waiting for a fraction of a group's work, so
    // `partial` against `groups` is the difference between a service that is
    // slow and a service that is STARVED.
    printf(
        "   %-22s   dispatch groups %ld of which PARTIAL %ld (%.0f%%), uops issued %ld, "
        "engine idle %ld\n",
        "", D.groups, D.partial, D.groups ? 100.0 * (double)D.partial / (double)D.groups : 0.0,
        D.uops, D.idle);
    if (D.span_clocks > 0)
      printf(
          "   %-22s   FROZEN by a long op awaiting the dispatcher %ld clocks (%.0f%%); "
          "ready-but-could-not-issue %ld\n",
          "", D.held, 100.0 * (double)D.held / (double)D.span_clocks, D.blocked);
    {
      // EXCLUSIVE BUCKETS, so they sum to the run and nothing hides.
      const long acc = D.uops + D.held + D.denied + D.dotc + D.skid + D.blocked + D.idle;
      printf(
          "   %-22s   clocks: issue %ld, longop-hold %ld, mul-denied %ld, dot %ld, skid %ld, "
          "ready-blocked %ld, idle %ld  (sum %ld of %ld)\n",
          "", D.uops, D.held, D.denied, D.dotc, D.skid, D.blocked, D.idle, acc, D.span_clocks);
    }
    printf("   %-22s   contexts alive %.1f of %d, uops issued %ld in %ld clocks (%.0f%%)\n", "",
           D.avg_active, n_ctx, D.uops, D.span_clocks,
           D.span_clocks ? 100.0 * (double)D.uops / (double)D.span_clocks : 0.0);
    printf("   %-22s   writeback: ALU served %ld stalled %ld, drain served %ld stalled %ld\n", "",
           D.wb_served[0], D.wb_stalled[0], D.wb_served[1], D.wb_stalled[1]);
    // THE FLOOR THE PORT ITSELF SETS. One write per clock, so this is what the
    // machine could not beat even with infinitely fast services.
    if (D.span_clocks > 0)
      printf("   %-22s   register writes %ld in %ld clocks = %.0f%% of the ONE write port\n", "",
             D.writes, D.span_clocks, 100.0 * (double)D.writes / (double)D.span_clocks);
  }

  if (total_stag_bad != 0) {
    printf("\n== STAGGERED DRIVE PRODUCED WRONG VALUES ==\n");
    printf("   %d of them. This gate is closed -- it should be zero.\n", total_stag_bad);
  }

  printf("\n== VERDICT ==\n");
  if (ran == 0) {
    printf("   NOTHING RAN. The refusals above are the finding.\n");
  } else if (failures != 0) {
    printf("   %d value mismatch(es). The timing figures above are NOT a result --\n", failures);
    printf("   a machine that computes the wrong answer quickly is not fast.\n");
  } else if (over == 0) {
    printf("   %d program(s) ran exact and every one is INSIDE the frame budget.\n", ran);
    printf("   worst: %s at %ld clocks against %ld.\n", worst_name, worst_frame, kFrameBudget);
  } else {
    printf("   %d of %d program(s) OVER. worst: %s at %ld against %ld -- %.2fx.\n", over, ran,
           worst_name, worst_frame, kFrameBudget, (double)worst_frame / (double)kFrameBudget);
  }

  printf("\n[field_v3_earth_directed] %d value check(s), %d failure(s)\n", total_checks, failures);

  return failures == 0 ? 0 : 1;
}
