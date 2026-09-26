// field_host_depth_census.cpp -- HOW OFTEN CAN THE COMPOSED FIELD HOST ACCEPT
// A POINT, AND HOW MANY EVALUATIONS OVERLAP? Measured on THE CONSOLE'S OWN
// HOST, at THE CONSOLE'S OWN PARAMETERS, and committed so it is reproducible.
//
// ===========================================================================
// WHY THIS FILE EXISTS: A DEPTH MAPPING THAT FOUR DOCUMENTS CARRY IS FALSE
// ===========================================================================
// `reports/DECISION-20260926-I34-FIELDMAJOR-BENCHED.md` prices the field-major
// Earth machine as `clocks(L) = 851 + (297/depth)*L`, where `depth` is the
// number of four-point GROUPS the engine holds in flight. It then maps two of
// its six depth columns onto real configurations, and the whole I34 build
// commission rests on that mapping:
//
//   "depth 2  : THE CONSOLE AS COMPOSED TODAY. `zhao_console_core.sv` gives
//               `u_field_host` `.FAB_LANES(1)` and leaves `PROGS` at its
//               default 8, so the executor is a SCALAR datapath with eight
//               contexts -- and a four-point group therefore occupies FOUR
//               contexts, i.e. two groups in flight."
//   "depth 32 : THE ENGINE'S OWN SHIPPED CONFIGURATION ... gated at
//               `-GCTX=32 -GLANES=4` ... At LANES=4 one context IS one
//               four-point group."
//
// Both sentences reason about `zhao_field_v3_engine`'s CONTEXT COUNT. Neither
// is about the block that stands between an adapter and that engine.
// `zhao_field_host_v2` is that block -- it is what `zhao_console_core.sv`
// instantiates as `u_field_host` -- and its front is a SINGLE sequential state
// machine:
//
//   E_IDLE -> E_ZERO -> E_WRITE -> E_START -> E_RUN -> E_DRAIN -> E_RETIRE
//
// with `req_ready_o` gated on `state == E_IDLE` (that file :1150). There is one
// `state` and one `cur_slot`, and neither is dimensioned by any parameter. So
// no `-G` value adds an evaluation, and a context count the front never reaches
// is not a depth.
//
// THE HOST'S OWN HEADER SAYS SO, in capitals, at the preload port:
//
//   "THE POINT IS REPLICATED ACROSS THE FABRIC'S LANES ... This front has
//    EXACTLY ONE POINT IN FLIGHT ... so at FAB_LANES>1 the other lanes
//    recompute the same point and their results are discarded. This is a
//    WASTE, not a fix ... A front that gathers FAB_LANES points per grant is
//    the thing that makes the width pay, AND IT IS NOT BUILT."
//
// And `design/contracts/GEOM.WARP.md` P5 measured it independently on
// 2026-09-21, five days BEFORE the census that contradicts it:
// "Parameter-independent across CLIENTS, PROGS, FAB_LANES (WHICH REPLICATES
// ONE POINT ACROSS LANES AND DISCARDS THE SURPLUS -- IT DOES NOT ADD POINTS),
// FAB_GROUP_PTS, FAB_OUTSTANDING and CREDITS."
//
// THAT IS THREE DOCUMENTS AGREEING, WHICH IS EXACTLY WHY THIS FILE MEASURES IT
// INSTEAD OF CITING THEM. `CLAUDE.md`: "a document cannot go stale loudly", and
// the "X does not exist" claims this campaign has killed came overwhelmingly
// out of documents rather than out of the tree. A depth of one is also the
// UNFLATTERING answer -- it says the commissioned build does not pay at any
// parameter setting -- so it is the one that has to be a reading.
//
// ===========================================================================
// WHAT IS MEASURED, AND THE ONE QUANTITY THAT MATTERS
// ===========================================================================
// The line is `clocks(L) = 851 + (297/depth)*L`. Written the way it binds,
// `L/depth` is THE STEADY-STATE INTERVAL BETWEEN ONE ACCEPTANCE AND THE NEXT,
// and 297 is the walker's own group count. So the interval is the whole of what
// has to be measured, and it reads straight off the client seam without ever
// being decomposed into a latency and a concurrency:
//
//   * ACCEPT-TO-ACCEPT INTERVAL, with every client offering continuously and
//     collecting every response immediately. This is `L/depth`, measured.
//   * RUN LATENCY: accept to that same run's own response. Reported so the
//     effective concurrency -- latency / interval -- is a DERIVED number with
//     both operands visible and separately measured, rather than an assumption.
//   * THE SAME PAIR WITH THE PER-POINT REGISTER CLEAR SUPPRESSED. E_ZERO is
//     REGS clocks of every run, and the host already has a documented way to
//     skip it: the image's INIT_PROOF (directive 6.1 / FH08). That column is
//     the one lever on the interval that needs no new block, and no document in
//     this tree had priced it.
//
// ===========================================================================
// AND THE FIRST VERSION OF THIS FILE GOT ITS OWN HEADLINE WRONG. RECORDED.
// ===========================================================================
// It asserted PEAK OUTSTANDING AT THE CLIENT SEAM == 1, reasoning from the
// single `state` register. The host answered TWO, and the host was right:
// `zhao_field_host_v2` RESERVES A RESPONSE ENTRY BEFORE ACCEPTANCE (FH20,
// `rsv_take_c`, CREDITS=2), so a FINISHED result can sit in the delivery queue
// while the next run is granted. Two requests really are outstanding.
//
// THEY DO NOT OVERLAP ANY ENGINE WORK, which is the distinction that was missed
// and the reason this is worth recording rather than merely fixing.
// `fieldmajor_census`'s `depth` is groups whose LATENCY overlaps -- its
// `Engine` model holds each slot for `latency` clocks and amortises exactly
// that. A retired result waiting to be collected amortises nothing, so counting
// it as depth would divide L by a number that buys no clocks.
// `design/contracts/GEOM.WARP.md` P5 had already said so in one clause --
// "CREDITS (whose overlap is a RETIRED, ALREADY-CAPTURED response waiting in
// the queue, downstream of `cur_export`)" -- and that clause is what the first
// version of this file failed to read.
//
// So the peak is still REPORTED, under its honest name, and what is ASSERTED is
// the derived engine concurrency, whose two operands are measured separately
// and by different events.
//
// ===========================================================================
// THE INSTRUMENT'S POSITIVE CONTROL, AND WHY IT IS NOT OPTIONAL
// ===========================================================================
// The headline is a concurrency reading ONE. `CLAUDE.md`: "a detector that has
// not been shown to FIRE has not been tested", and an accumulator that never
// updated would report the same one.
//
// So the accounting is factored into `Depth`, a plain object with no knowledge
// of the DUT, and case 0 drives it from a SYNTHETIC trace in which two and then
// three runs genuinely overlap. It must report those before any reading of the
// real host is believed -- and it must report 1, not 8, for eight strictly
// serial runs, which is the half that separates a concurrency from a total. The
// control and the measurement share one implementation by construction, which
// is the only arrangement in which the control is evidence about the
// measurement.
//
// NOTE WHAT IS *NOT* ASSERTED. This file does not assert "the field-major
// machine misses its contract". That would be asserting a defect, which
// `CLAUDE.md` forbids: once a gathering front exists the assertion would have
// to be deleted rather than satisfied. It asserts the host's CORRECT present
// behaviour, so a front that changes it turns this test red and names the
// number, which is the point.
//
// ===========================================================================
// ONE LOAD-ORDER LAW, FOUND BY THIS FILE AND WORTH THE LINE
// ===========================================================================
// `LdInitProof` sets `hdr_loaded[slot] <= 1'b0` (`zhao_field_host_v2.sv:1472`),
// so it INVALIDATES the header. Loading the proof after the header makes every
// subsequent run answer `StNoProgram` (0xF0) -- instantly, at one clock per
// refusal, which reads as a spectacular speed-up. The first draft of case 2 did
// exactly that and reported "1.00 clocks per run"; the status check below is
// what caught it, and it is why that check exists rather than only a count.

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

// THE COMPOSED SHAPE. These are `zhao_console_core.sv`'s OWN arguments to
// `u_field_host`, not this module's defaults -- a bench measuring the bench's
// parameters would answer about nothing, which is the mistake
// `zhao_field_host.sv:115` names ("a fit that does not override them is
// measuring the bench"). The CMake entry passes the matching -G values.
constexpr int kClients = 4;
constexpr int kOrdinals = 7;
constexpr int kInLanes = 15;
constexpr int kRegs = 32;

// The walker's own group count, measured by `zhao_terrain_field_walk` and
// derived twice independently in the owner directive (2.10, 13.5):
// 33 * ceil(33/4) = 297 row-bounded quad groups.
constexpr double kGroups = 297.0;

// The active per-association contract: design/contracts/FIELD.SEQ.EARTH.md:167.
constexpr double kContract = 6000.0;

// The field-major floor `fieldmajor_census` measured with both ends group-wide.
constexpr double kFloor = 851.0;

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

void set_req_in(Dut& d, int client, int lane, int32_t value) {
  d.req_in_i[client * kInLanes + lane] = static_cast<uint32_t>(value);
}

void set_req_slot(Dut& d, int client, uint8_t slot) {
  uint32_t v = d.req_slot_i;
  v &= ~(0x7u << (client * 3));
  v |= (static_cast<uint32_t>(slot & 0x7) << (client * 3));
  d.req_slot_i = v;
}

// ---------------------------------------------------------------------------
// THE ACCOUNTING. Deliberately knows nothing about the DUT, so case 0 can drive
// it from a synthetic trace and prove it can report more than one.
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
  long peak_outstanding = 0;   // client seam, INCLUDING the retired-response queue
  long runs = 0;               // responses actually collected
  long clocks = 0;
  double accept_interval = 0.0;  // THE HEADLINE: L/depth, in clocks
  double run_latency = 0.0;      // accept -> that run's own response
  double engine_overlap = 0.0;   // DERIVED: latency / interval
  uint8_t last_status = 0xFF;
  int32_t last_out0 = 0;
  long latency_samples = 0;
};

// Offer from every client continuously, collect every response immediately, and
// watch the two handshakes. `runs` is counted at the RESPONSE, so a run that was
// accepted and never answered cannot inflate it.
//
// `warmup` accepts are discarded before the interval is timed, so the first
// grant out of reset does not enter the steady-state average.
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
        c.last_out0 = static_cast<int32_t>(d.resp_out_o[0]);
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
  c.runs = dep.answers;
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
  std::printf("    runs collected                : %ld in %ld clocks\n", c.runs, c.clocks);
  std::printf("    ACCEPT-TO-ACCEPT INTERVAL     : %.2f clocks   <- this is L/depth\n",
              c.accept_interval);
  std::printf("    run latency (accept->response): %.2f clocks\n", c.run_latency);
  std::printf("    ENGINE OVERLAP (derived)      : %.2f evaluations\n", c.engine_overlap);
  std::printf("    peak outstanding at the seam  : %ld  (INCLUDES the retired-response\n",
              c.peak_outstanding);
  std::printf("                                      queue, which overlaps no engine work)\n");
  std::printf("    last status                   : 0x%02X\n", c.last_status);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  // =======================================================================
  // CASE 0. THE INSTRUMENT'S POSITIVE CONTROL, RUN BEFORE THE DUT IS TOUCHED.
  // =======================================================================
  // The headline below is a concurrency of ONE. An accumulator that never
  // updated would say the same thing, and would look exactly like this result.
  {
    Depth dep;
    dep.accept(1);
    dep.accept(1);
    check(dep.peak == 2, "case 0: two overlapping runs read as peak 2", 2,
          static_cast<uint64_t>(dep.peak));
    dep.accept(1);
    check(dep.peak == 3, "case 0: three overlapping runs read as peak 3", 3,
          static_cast<uint64_t>(dep.peak));
    dep.answer(3);
    dep.accept(1);
    check(dep.peak == 3, "case 0: the peak is a HIGH-WATER mark, not the current depth", 3,
          static_cast<uint64_t>(dep.peak));
    check(dep.accepts == 4, "case 0: accepts counted", 4, static_cast<uint64_t>(dep.accepts));
    check(dep.answers == 3, "case 0: answers counted", 3, static_cast<uint64_t>(dep.answers));

    // AND THE NEGATIVE HALF: without it, an accumulator reporting the TOTAL
    // rather than the concurrency would pass every check above.
    Depth serial;
    for (int i = 0; i < 8; ++i) {
      serial.accept(1);
      serial.answer(1);
    }
    check(serial.peak == 1, "case 0: eight strictly SERIAL runs read as peak 1, not 8", 1,
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
  for (int i = 0; i < kClients * kInLanes; ++i) dut.req_in_i[i] = 0;
  reset(dut, 4);

  // A two-instruction canonical program on slot 1: ordinal 0 = R0 + R1. The
  // ARITHMETIC is not under test here; the CADENCE is. A short program is the
  // CONSERVATIVE choice for this measurement -- it makes the run as cheap as
  // this host can make it, so the interval measured is a FLOOR and a real Earth
  // program can only be slower.
  const uint8_t slot = 1, ob = 2;
  check(load_word(dut, kLdUop, slot, 0, instr(kOpAdd, ob, 0, 1, 0, 0)), "program uop 0", 1, 1);
  check(load_word(dut, kLdUop, slot, 1, instr(kOpEnd, 0, 0, 0, 0, 0)), "program end", 1, 1);
  check(load_outmap(dut, slot, 0, kSrcVectorReg, ob), "program outmap ordinal 0", 1, 1);
  check(load_assoc(dut, slot, 1), "program assoc", 1, 1);
  check(load_word(dut, kLdHeader, slot, 0, header_word(2, ob, 0x01, 0x01, 1, kFormCanonical)),
        "program header", 1, 1);

  for (int cl = 0; cl < kClients; ++cl) {
    set_req_slot(dut, cl, slot);
    set_req_in(dut, cl, 0, 11);
    set_req_in(dut, cl, 1, 31);
  }

  // =======================================================================
  // CASE 1. THE CONSOLE'S HOST, EVERY CLIENT OFFERING AT ONCE.
  // =======================================================================
  // THE CASE THAT DISCRIMINATES: four clients each holding `req_valid_i` high
  // for the whole window is the most concurrency this seam can be offered. A
  // host with real outstanding capacity would show an overlap above one here.
  // This is the reading that "depth 2, the console as composed today" is a
  // claim about.
  Census slow = saturate(dut, 4000, kClients);
  report("THE CONSOLE'S COMPOSED HOST, SATURATED (per-point clear ON)", slow);

  check(slow.runs > 0, "case 1: the host actually ran -- a zero makes every other number vacuous",
        1, slow.runs > 0 ? 1 : 0);
  check(slow.last_status == kStOk,
        "case 1: the runs SUCCEEDED (StOk), so this is the cadence of WORK, not of refusals",
        kStOk, slow.last_status);
  check(slow.last_out0 == 42, "case 1: ... and answered R0+R1, so the engine really evaluated", 42,
        static_cast<uint64_t>(slow.last_out0));
  check(slow.latency_samples > 0, "case 1: run latency was sampled", 1,
        slow.latency_samples > 0 ? 1 : 0);
  check(slow.engine_overlap < 1.5,
        "case 1: THE COMPOSED HOST OVERLAPS AT MOST ONE FIELD EVALUATION", 1,
        (slow.engine_overlap < 1.5) ? 1 : 0);

  // =======================================================================
  // CASE 2. THE SAME, WITH THE PER-POINT REGISTER CLEAR SUPPRESSED.
  // =======================================================================
  // THE CASE THAT DISCRIMINATES: E_ZERO is REGS clocks of every run, and
  // directive 6.1 / FH08 already allow an image with an accepted INIT_PROOF to
  // skip it. If the saving were not real the two intervals would agree -- and a
  // field-major front that reloaded the whole register file per group would be
  // paying it 297 times per association.
  //
  // THE HEADER IS RELOADED AFTER THE PROOF, because `LdInitProof` clears
  // `hdr_loaded`. Without that the host answers StNoProgram at one clock per
  // refusal and the interval collapses to 1.00 -- a 62x "speed-up" that is
  // entirely refusals. The status check is what separates the two.
  check(load_initproof(dut, slot, true), "case 2: INIT_PROOF accepted for the slot", 1, 1);
  check(load_word(dut, kLdHeader, slot, 0, header_word(2, ob, 0x01, 0x01, 1, kFormCanonical)),
        "case 2: header RELOADED after the proof (LdInitProof clears hdr_loaded)", 1, 1);
  Census fast = saturate(dut, 4000, kClients);
  report("THE SAME HOST WITH THE PER-POINT REGISTER CLEAR SKIPPED", fast);

  check(fast.runs > 0, "case 2: the host ran with the clear skipped", 1, fast.runs > 0 ? 1 : 0);
  check(fast.last_status == kStOk,
        "case 2: still StOk -- the fast path is running the program, not refusing it", kStOk,
        fast.last_status);
  check(fast.last_out0 == 42, "case 2: ... and still answers R0+R1", 42,
        static_cast<uint64_t>(fast.last_out0));
  check(fast.engine_overlap < 1.5,
        "case 2: THE OVERLAP IS UNCHANGED BY THE CLEAR -- it is the FRONT, not the work", 1,
        (fast.engine_overlap < 1.5) ? 1 : 0);
  check(fast.accept_interval < slow.accept_interval,
        "case 2: skipping the per-point clear really is cheaper, so the lever is real", 1,
        (fast.accept_interval < slow.accept_interval) ? 1 : 0);
  // The saving is REGS clocks, give or take the state the two paths share. This
  // is the check that would catch a future REGS change silently repricing it.
  const double saved = slow.accept_interval - fast.accept_interval;
  check(saved > (kRegs * 0.8) && saved < (kRegs * 1.2),
        "case 2: and the saving is REGS clocks, within 20%", kRegs,
        static_cast<uint64_t>(saved));

  // =======================================================================
  // WHAT THE NUMBERS MEAN FOR THE FIELD-MAJOR LINE.
  // =======================================================================
  // REPORTED, NOT ASSERTED. `fieldmajor_census` owns the line; this file owns
  // the two inputs the line was given wrong. Asserting a verdict here would be
  // asserting the defect, which `CLAUDE.md` forbids.
  //
  // A GROUP IS FOUR POINTS AND THIS FRONT ANSWERS ONE POINT PER RUN
  // (`fab_pre_data` replicates: `{FAB_LANES{cur_in[lane_sel]}}`), so a
  // four-point group costs FOUR accept intervals until a gathering front
  // exists. That multiplier is the honest one to carry forward and it is the
  // reason the walker's four-wide output does not, on its own, buy anything.
  const double pts_per_group = 4.0;
  const double grp_slow = slow.accept_interval * pts_per_group;
  const double grp_fast = fast.accept_interval * pts_per_group;
  std::printf("\n  === WHAT THIS MEANS FOR `clocks(L) = 851 + (297/depth)*L` ===\n");
  std::printf("    engine overlap, MEASURED on the composed host : %.2f\n", slow.engine_overlap);
  std::printf("    points per run on this front                  : 1 (lanes replicate)\n");
  std::printf("    clocks per four-point GROUP  (clear ON)       : %.0f\n", grp_slow);
  std::printf("    clocks per four-point GROUP  (clear SKIPPED)  : %.0f\n", grp_fast);
  std::printf("    297 groups + the 851 floor   (clear ON)       : %.0f clocks = %.2fx 6,000\n",
              kFloor + kGroups * grp_slow, (kFloor + kGroups * grp_slow) / kContract);
  std::printf("    297 groups + the 851 floor   (clear SKIPPED)  : %.0f clocks = %.2fx 6,000\n",
              kFloor + kGroups * grp_fast, (kFloor + kGroups * grp_fast) / kContract);
  std::printf("    (the active contract is <= 6,000 clocks per full association:\n");
  std::printf("     design/contracts/FIELD.SEQ.EARTH.md:167)\n");
  std::printf("\n    THE BUDGET SAID THE OTHER WAY ROUND: to reach 6,000 clocks with the\n");
  std::printf("    851 floor and 297 groups, a group must cost <= %.1f clocks.\n",
              (kContract - kFloor) / kGroups);

  // Verilated mains on this toolchain must exit through `exit_hard` -- see
  // `zhao_sim.hpp`: VlThreadPool's destructor deadlocks at static teardown.
  zhao::exit_hard(zhao::report_and_exit("field_host_depth_census"));
}
