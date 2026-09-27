// field_gather_front_census.cpp -- THE GATHERING FRONT, MEASURED. How many
// clocks does a FOUR-POINT VECTOR GROUP cost when one engine run covers the
// whole group instead of one point of it?
//
// ===========================================================================
// WHAT THIS IS THE OTHER HALF OF
// ===========================================================================
// `field_host_depth_census.cpp` measured the console's composed host and found
// the thing every document in this tree had got wrong: the front holds ONE
// POINT IN FLIGHT, at every parameter setting, so a four-point group costs FOUR
// engine round trips and 248 clocks against a budget of 17.3. It named the
// missing piece in `zhao_field_host_v2.sv`'s own words --
//
//   "A front that gathers FAB_LANES points per grant is the thing that makes
//    the width pay, AND IT IS NOT BUILT."
//
// It is built now, as `FRONT_PTS` on that module, and this file is its
// measurement. The two files are deliberately separate executables because they
// measure DIFFERENT MACHINES: the depth census elaborates the host at the
// console's own `-GFAB_LANES=1 -GFRONT_PTS=1` and must keep doing so, or the
// number it reports stops being about the console. This one elaborates
// `-GFAB_LANES=4 -GFRONT_PTS=4`, which is a machine the console does NOT
// compose today, and says so in every figure it prints.
//
// ===========================================================================
// THE ONE CHECK THAT SEPARATES A GATHERING FRONT FROM THE FRONT IT REPLACES
// ===========================================================================
// FOUR DIFFERENT POINTS MUST PRODUCE FOUR DIFFERENT ANSWERS.
//
// The front this replaces wrote `{FAB_LANES{cur_in[lane_sel]}}` -- ONE point
// replicated into every lane -- and read lane 0 back. Such a front, given four
// different points, answers the FIRST point's value four times. It would pass a
// cadence check, a run-count check, a status check and an overlap check. It
// would pass "the output is 42". The ONLY check it fails is the one that asks
// whether the other three points were evaluated at all.
//
// That is `CLAUDE.md`'s own lesson about this subsystem arriving twice in two
// days: the first draft of the depth census measured a 62x speed-up made
// entirely of `StNoProgram` refusals, and "the run-count check PASSED; only the
// status check caught it." A replicating front is the same defect one level
// deeper -- every status is `StOk`, every run is real work, and three quarters
// of the answers are the wrong point's.
//
// SO THE CHECK IS SEEN TO FAIL, not argued to be sound. Points differ by lane
// index, so the four answers are 42, 43, 44, 45 and a replicating front says
// 42, 42, 42, 42. `tests/mutants/zhao_field_host_v2_gather_replicate_mutant.sv`
// is that front, committed, with its driver's polarity inverted: it PASSES when
// the four answers COLLAPSE. The mutation is one line.
//
// ===========================================================================
// WHAT RAISING `FAB_LANES` COSTS, SAID BEFORE THE NUMBERS RATHER THAN AFTER
// ===========================================================================
// `FRONT_PTS` cannot exceed `FAB_LANES` -- point p lives in fabric lane p, and
// the module's elaboration guard refuses the other arrangement. So this
// measurement is taken at `FAB_LANES=4` where the console composes 1, and that
// is a REAL COST and not a free parameter: `zhao_field_v3_exec.sv`'s header
// says "LANES widens the DATAPATH and nothing else ... only operands, results
// and products carry four values instead of one", so four lanes are four ALU
// and register-file replicas. THIS FILE DOES NOT MEASURE THAT AREA. Verilator
// answers clocks; ALMs and DSPs are a fit, and the fit is the coordinator's.
// The figures below are therefore a THROUGHPUT result that carries an
// unmeasured area bill, and quoting them without the bill would be this
// campaign's own "component checks passing is not likeness evidence".
//
// ===========================================================================
// AND WHAT IS NOT ASSERTED
// ===========================================================================
// No assertion here asserts the 6,000-clock contract in either direction. The
// contract figures are PRINTED. Asserting "the gathering front meets it" would
// bake a budget into a correctness test, and asserting "it misses it" would be
// asserting a defect that the next lever is meant to remove -- which
// `CLAUDE.md` forbids by name. What is asserted is the machine's CORRECT
// behaviour: four points in, four right answers out, at StOk.

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

// The host's load kinds.
constexpr uint8_t kLdUop = 0;
constexpr uint8_t kLdHeader = 2;
constexpr uint8_t kLdOutMap = 4;
constexpr uint8_t kLdAssoc = 5;
constexpr uint8_t kLdInitProof = 6;

constexpr uint8_t kSrcVectorReg = 0;
constexpr uint8_t kFormCanonical = 0;
constexpr uint8_t kStOk = 0x00;

// THE COMPOSED SHAPE, with the two knobs this file exists to move. Everything
// except FAB_LANES and FRONT_PTS is `zhao_console_core.sv`'s own argument to
// `u_field_host`; the CMake entry passes the matching -G values.
constexpr int kClients = 4;
constexpr int kOrdinals = 7;
constexpr int kInLanes = 15;
constexpr int kRegs = 32;
constexpr int kFrontPts = 4;  // -GFRONT_PTS=4, -GFAB_LANES=4

// The walker's own group count: 33 * ceil(33/4) = 297 row-bounded quad groups.
constexpr double kGroups = 297.0;

// The active per-association contract: design/contracts/FIELD.SEQ.EARTH.md:167.
constexpr double kContract = 6000.0;

// The field-major floor `fieldmajor_census` measured with both ends group-wide.
constexpr double kFloor = 851.0;

// What `field_host_depth_census` measured on the SCALAR front, at this same
// console parameterisation. Printed for comparison, never asserted -- a test
// that asserts another test's number is comparing to a measurement it cannot
// see, which is `CLAUDE.md`'s "never compare a current file to an old
// measurement".
constexpr double kScalarGroupClocks = 248.0;

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

//   [13:8] out_base  [22:16] tbl_n0  [31:24] tbl_n1
//   [32 +: 7] WINDOW mask  [48 +: 8] ORDINAL mask
//   [56 +: 4] output_count  [60 +: 2] execution_form
uint64_t header_word(uint8_t instr_count, uint8_t out_base, uint8_t win_mask, uint8_t ord_mask,
                     uint8_t out_count, uint8_t form) {
  uint64_t w = static_cast<uint64_t>(instr_count) | (static_cast<uint64_t>(out_base & 0x3F) << 8);
  w |= (static_cast<uint64_t>(win_mask) & 0x7Full) << 32;
  w |= (static_cast<uint64_t>(ord_mask) & 0xFFull) << 48;
  w |= (static_cast<uint64_t>(out_count) & 0x0Full) << 56;
  w |= (static_cast<uint64_t>(form) & 0x03ull) << 60;
  return w;
}

bool load_outmap(Dut& d, uint8_t slot, uint8_t ordinal, uint8_t kind, uint16_t src) {
  uint64_t w = static_cast<uint64_t>(src) | (static_cast<uint64_t>(kind & 1) << 16);
  return load_word(d, kLdOutMap, slot, ordinal, w);
}

bool load_assoc(Dut& d, uint8_t slot, uint8_t gen) {
  return load_word(d, kLdAssoc, slot, 0, static_cast<uint64_t>(gen));
}

bool load_initproof(Dut& d, uint8_t slot, bool ok) {
  return load_word(d, kLdInitProof, slot, 0, ok ? 1ull : 0ull);
}

// `req_in_i` is POINT-MAJOR then LANE-MAJOR: client c point p lane l is word
// ((c*FRONT_PTS + p)*IN_LANES + l).
void set_req_in(Dut& d, int client, int pt, int lane, int32_t value) {
  d.req_in_i[(client * kFrontPts + pt) * kInLanes + lane] = static_cast<uint32_t>(value);
}

// `resp_out_o` is POINT-MAJOR then ORDINAL-MAJOR.
int32_t get_resp_out(Dut& d, int pt, int ordinal) {
  return static_cast<int32_t>(d.resp_out_o[pt * kOrdinals + ordinal]);
}

void set_req_slot(Dut& d, int client, uint8_t slot) {
  uint32_t v = d.req_slot_i;
  v &= ~(0x7u << (client * 3));
  v |= (static_cast<uint32_t>(slot & 0x7) << (client * 3));
  d.req_slot_i = v;
}

// ---------------------------------------------------------------------------
// THE ACCOUNTING. The same shape as the depth census's, and deliberately
// ignorant of the DUT so case 0 can prove it reports more than one.
// ---------------------------------------------------------------------------
struct Depth {
  long outstanding = 0;
  long peak = 0;
  long accepts = 0;
  long answers = 0;

  void accept(int n) {
    outstanding += n;
    accepts += n;
    if (outstanding > peak) peak = outstanding;
  }
  void answer(int n) {
    outstanding -= n;
    answers += n;
  }
};

struct Census {
  long peak_outstanding = 0;
  long groups = 0;               // RESPONSES, each covering FRONT_PTS points
  long clocks = 0;
  double accept_interval = 0.0;  // AT FRONT_PTS>1 THIS IS CLOCKS PER GROUP
  double run_latency = 0.0;
  double engine_overlap = 0.0;
  uint8_t last_status = 0xFF;
  int32_t last_out[kFrontPts] = {0, 0, 0, 0};
  long latency_samples = 0;
  // THE DISCRIMINATOR: responses in which all FRONT_PTS points answered the
  // SAME value. On distinct inputs that is a replicating front.
  long collapsed_responses = 0;
  long distinct_responses = 0;
};

// Offer from every client continuously, collect every response immediately.
// `warmup` accepts are discarded before the interval is timed.
Census saturate(Dut& d, int clocks, int clients, int warmup = 2) {
  Census c;
  Depth dep;
  const uint32_t all = (clients >= 32) ? 0xFFFFFFFFu : ((1u << clients) - 1u);

  d.req_valid_i = all;
  d.resp_ready_i = all;

  std::vector<long> accepted_at(static_cast<size_t>(clients), -1);
  long timed_first = -1, timed_last = -1, timed_accepts = 0;
  long latency_total = 0;

  for (int t = 0; t < clocks; ++t) {
    d.eval();
    const uint32_t acc = static_cast<uint32_t>(d.req_ready_o) & all;
    const uint32_t ans = static_cast<uint32_t>(d.resp_valid_o) & all;

    for (int i = 0; i < clients; ++i) {
      if (acc & (1u << i)) {
        dep.accept(1);
        accepted_at[static_cast<size_t>(i)] = t;
        if (dep.accepts > warmup) {
          if (timed_first < 0) timed_first = t;
          timed_last = t;
          ++timed_accepts;
        }
      }
    }
    for (int i = 0; i < clients; ++i) {
      if (ans & (1u << i)) {
        dep.answer(1);
        c.last_status = d.resp_status_o;
        bool all_same = true;
        for (int p = 0; p < kFrontPts; ++p) {
          c.last_out[p] = get_resp_out(d, p, 0);
          if (c.last_out[p] != c.last_out[0]) all_same = false;
        }
        if (all_same) {
          ++c.collapsed_responses;
        } else {
          ++c.distinct_responses;
        }
        const long a = accepted_at[static_cast<size_t>(i)];
        if (a >= 0) {
          latency_total += (t - a);
          ++c.latency_samples;
          accepted_at[static_cast<size_t>(i)] = -1;
        }
      }
    }
    step(d);
  }

  d.req_valid_i = 0;
  d.resp_ready_i = 0;

  c.peak_outstanding = dep.peak;
  c.groups = dep.answers;
  c.clocks = clocks;
  if (timed_accepts > 1 && timed_last > timed_first) {
    c.accept_interval =
        static_cast<double>(timed_last - timed_first) / static_cast<double>(timed_accepts - 1);
  }
  if (c.latency_samples > 0) {
    c.run_latency = static_cast<double>(latency_total) / static_cast<double>(c.latency_samples);
  }
  if (c.accept_interval > 0.0) c.engine_overlap = c.run_latency / c.accept_interval;
  return c;
}

void report(const char* title, const Census& c) {
  std::printf("\n  === %s ===\n", title);
  std::printf("    GROUPS collected              : %ld in %ld clocks (%d points each)\n", c.groups,
              c.clocks, kFrontPts);
  std::printf("    CLOCKS PER FOUR-POINT GROUP   : %.2f   <- accept-to-accept\n",
              c.accept_interval);
  std::printf("    group latency (accept->resp)  : %.2f clocks\n", c.run_latency);
  std::printf("    ENGINE OVERLAP (derived)      : %.2f groups\n", c.engine_overlap);
  std::printf("    peak outstanding at the seam  : %ld  (INCLUDES the retired-response\n",
              c.peak_outstanding);
  std::printf("                                      queue, which overlaps no engine work)\n");
  std::printf("    last status                   : 0x%02X\n", c.last_status);
  std::printf("    last group's four answers     : %d %d %d %d\n", c.last_out[0], c.last_out[1],
              c.last_out[2], c.last_out[3]);
  std::printf("    responses with DISTINCT values: %ld   (collapsed: %ld)\n", c.distinct_responses,
              c.collapsed_responses);
}

void budget(const char* title, double grp) {
  const double assoc = kFloor + kGroups * grp;
  std::printf("    %-34s %7.2f clk/group -> %8.0f clocks = %.2fx 6,000\n", title, grp, assoc,
              assoc / kContract);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  // =======================================================================
  // CASE 0. THE INSTRUMENT'S POSITIVE CONTROL, BEFORE THE DUT IS TOUCHED.
  // =======================================================================
  // The overlap reported below is a number near one, and an accumulator that
  // never updated would report the same thing.
  {
    Depth dep;
    dep.accept(1);
    dep.accept(1);
    check(dep.peak == 2, "case 0: two overlapping groups read as peak 2", 2,
          static_cast<uint64_t>(dep.peak));
    dep.accept(1);
    check(dep.peak == 3, "case 0: three overlapping groups read as peak 3", 3,
          static_cast<uint64_t>(dep.peak));
    dep.answer(3);
    dep.accept(1);
    check(dep.peak == 3, "case 0: the peak is a HIGH-WATER mark, not the current depth", 3,
          static_cast<uint64_t>(dep.peak));
    Depth serial;
    for (int i = 0; i < 8; ++i) {
      serial.accept(1);
      serial.answer(1);
    }
    check(serial.peak == 1, "case 0: eight strictly SERIAL groups read as peak 1, not 8", 1,
          static_cast<uint64_t>(serial.peak));
    check(serial.accepts == 8, "case 0: ... while all eight are still counted", 8,
          static_cast<uint64_t>(serial.accepts));
  }

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
  for (int i = 0; i < kClients * kFrontPts * kInLanes; ++i) dut.req_in_i[i] = 0;
  reset(dut, 4);

  // A two-instruction canonical program on slot 1: ordinal 0 = R0 + R1. Same
  // program as the depth census's, for the same reason: the ARITHMETIC is not
  // under test, the CADENCE is, and the shortest program makes the measured
  // interval a FLOOR that a real Earth program can only exceed.
  const uint8_t slot = 1, ob = 2;
  check(load_word(dut, kLdUop, slot, 0, instr(kOpAdd, ob, 0, 1, 0, 0)), "program uop 0", 1, 1);
  check(load_word(dut, kLdUop, slot, 1, instr(kOpEnd, 0, 0, 0, 0, 0)), "program end", 1, 1);
  check(load_outmap(dut, slot, 0, kSrcVectorReg, ob), "program outmap ordinal 0", 1, 1);
  check(load_assoc(dut, slot, 1), "program assoc", 1, 1);
  check(load_word(dut, kLdHeader, slot, 0, header_word(2, ob, 0x01, 0x01, 1, kFormCanonical)),
        "program header", 1, 1);

  // FOUR DIFFERENT POINTS PER GROUP. R1 = 31 for all of them and R0 walks
  // 11,12,13,14, so the four answers must be 42,43,44,45 -- and a front that
  // replicates point 0 answers 42 four times. This is the whole discriminator.
  for (int cl = 0; cl < kClients; ++cl) {
    set_req_slot(dut, cl, slot);
    for (int p = 0; p < kFrontPts; ++p) {
      set_req_in(dut, cl, p, 0, 11 + p);
      set_req_in(dut, cl, p, 1, 31);
    }
  }

  // =======================================================================
  // CASE 1. THE GATHERING FRONT, EVERY CLIENT OFFERING AT ONCE.
  // =======================================================================
  Census slow = saturate(dut, 4000, kClients);
  report("THE GATHERING FRONT, SATURATED (per-point clear ON)", slow);

  check(slow.groups > 0, "case 1: the host actually ran -- a zero makes every other number vacuous",
        1, slow.groups > 0 ? 1 : 0);
  check(slow.last_status == kStOk,
        "case 1: the runs SUCCEEDED (StOk), so this is the cadence of WORK, not of refusals",
        kStOk, slow.last_status);

  // ---- THE DISCRIMINATING CHECK -----------------------------------------
  // Four points in, FOUR DIFFERENT ANSWERS out. See the header: this is the
  // only check a replicating front fails, and the committed mutant
  // `zhao_field_host_v2_gather_replicate_mutant.sv` is it failing.
  for (int p = 0; p < kFrontPts; ++p) {
    check(slow.last_out[p] == 42 + p,
          "case 1: point p of the group answered R0+R1 for ITS OWN R0, not point 0's",
          static_cast<uint64_t>(42 + p), static_cast<uint64_t>(slow.last_out[p]));
  }
  check(slow.distinct_responses > 0,
        "case 1: ... and that held for whole responses, not one lucky sample", 1,
        slow.distinct_responses > 0 ? 1 : 0);
  check(slow.collapsed_responses == 0,
        "case 1: NO response collapsed to one repeated value -- the front gathers", 0,
        static_cast<uint64_t>(slow.collapsed_responses));

  check(slow.engine_overlap < 1.5,
        "case 1: the front still holds ONE group in flight -- gathering is not pipelining", 1,
        (slow.engine_overlap < 1.5) ? 1 : 0);

  // =======================================================================
  // CASE 2. THE SAME, WITH THE PER-POINT REGISTER CLEAR SUPPRESSED (FH08).
  // =======================================================================
  // THE HEADER IS RELOADED AFTER THE PROOF, because `LdInitProof` sets
  // `hdr_loaded[slot] <= 1'b0` (`zhao_field_host_v2.sv`, the `LdInitProof` arm)
  // and therefore INVALIDATES the header. Loading the proof last makes every
  // run answer StNoProgram in one clock -- an enormous "speed-up" made entirely
  // of refusals, which is what the depth census's first draft measured. The
  // status and VALUE checks below are what separate the two.
  check(load_initproof(dut, slot, true), "case 2: INIT_PROOF accepted for the slot", 1, 1);
  check(load_word(dut, kLdHeader, slot, 0, header_word(2, ob, 0x01, 0x01, 1, kFormCanonical)),
        "case 2: header RELOADED after the proof (LdInitProof clears hdr_loaded)", 1, 1);
  Census fast = saturate(dut, 4000, kClients);
  report("THE GATHERING FRONT + INIT_PROOF (per-point clear SKIPPED)", fast);

  check(fast.groups > 0, "case 2: the host ran with the clear skipped", 1, fast.groups > 0 ? 1 : 0);
  check(fast.last_status == kStOk,
        "case 2: still StOk -- the fast path is RUNNING the program, not refusing it", kStOk,
        fast.last_status);
  for (int p = 0; p < kFrontPts; ++p) {
    check(fast.last_out[p] == 42 + p,
          "case 2: ... and every point still answers its OWN value on the fast path",
          static_cast<uint64_t>(42 + p), static_cast<uint64_t>(fast.last_out[p]));
  }
  check(fast.collapsed_responses == 0,
        "case 2: no response collapsed on the fast path either", 0,
        static_cast<uint64_t>(fast.collapsed_responses));
  check(fast.accept_interval < slow.accept_interval,
        "case 2: skipping the per-point clear really is cheaper, so the lever is real", 1,
        (fast.accept_interval < slow.accept_interval) ? 1 : 0);

  // THE SAVING IS REGS CLOCKS **PER GROUP**, NOT PER POINT, AND THAT IS THE
  // POINT OF COMPOSING THE TWO LEVERS. On the scalar front E_ZERO was paid four
  // times per group; here it is paid once. This check would catch a future REGS
  // change silently repricing it.
  const double saved = slow.accept_interval - fast.accept_interval;
  check(saved > (kRegs * 0.8) && saved < (kRegs * 1.2),
        "case 2: and the saving is REGS clocks PER GROUP, within 20%", kRegs,
        static_cast<uint64_t>(saved));

  // =======================================================================
  // CASE 3. THE ORDINAL SEAM IS STILL THE ORDINAL SEAM.
  // =======================================================================
  // A widened response port is exactly where an ordinal could silently become a
  // point index. `resp_count_o` and `resp_present_o` are NOT widened by
  // FRONT_PTS -- the lanes share one instruction stream, so which ordinals were
  // written is one answer for the group -- and this checks that decision rather
  // than leaving it in a comment.
  check(dut.resp_count_o == 1, "case 3: the declared output count is still ONE ordinal", 1,
        static_cast<uint64_t>(dut.resp_count_o));

  // =======================================================================
  // WHAT THE NUMBERS MEAN FOR THE ASSOCIATION BUDGET. REPORTED, NOT ASSERTED.
  // =======================================================================
  std::printf("\n  === CLOCKS PER ASSOCIATION: 851 floor + 297 four-point groups ===\n");
  std::printf("    (the active contract is <= 6,000 clocks per full association:\n");
  std::printf("     design/contracts/FIELD.SEQ.EARTH.md:167. A group must cost <= %.1f.)\n\n",
              (kContract - kFloor) / kGroups);
  budget("scalar front (depth census)", kScalarGroupClocks);
  budget("GATHERING FRONT", slow.accept_interval);
  budget("GATHERING FRONT + INIT_PROOF", fast.accept_interval);
  std::printf("\n    MEASURED HERE AT -GFAB_LANES=4 -GFRONT_PTS=4. The console composes\n");
  std::printf("    FAB_LANES=1 and does not compose this front. Four lanes are four\n");
  std::printf("    datapath replicas, and THIS FILE STILL MEASURES NO AREA -- but the\n");
  std::printf("    area is no longer unmeasured. Packet LANESCOST priced it on\n");
  std::printf("    2026-09-27, four leaf -MapOnly rows on the shipping 5CSEBA6U23I7:\n");
  std::printf("      composing this front costs +11,979 ALUTs (+14.3%% of the part),\n");
  std::printf("      +9 DSP (+8.0%%), +10,472 registers, +75,648 memory bits.\n");
  std::printf("      reports/synthesis/receipts/lanescost_field_host_v2_lanes_ladder.json\n");
  std::printf("      reports/DECISION-20260927-I34-LANESCOST-PRICED-AND-REFUSED.md\n");
  std::printf("    AND THE SURPRISE IS WHICH PARAMETER IS EXPENSIVE: FAB_LANES is only\n");
  std::printf("    +5,070 of that (+11%%, not the +300%% a 4x replica implies), because\n");
  std::printf("    the instruction stream and control are SHARED and the mulbank always\n");
  std::printf("    computed four lanes with three tied off. FRONT_PTS (+3,975) and the\n");
  std::printf("    FAB_GROUP_PTS tax (+2,934) together cost MORE than the lanes do.\n");

  zhao::exit_hard(zhao::report_and_exit("field_gather_front_census"));
}
