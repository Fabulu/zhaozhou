// field_stamp_adapter_directed.cpp — the S profile's stream adapter: does it
// walk the consumer's order, and does it stay in step with it?
//
// THE QUESTION THIS FILE ANSWERS is alignment, because alignment is the only
// thing this block can get wrong. It computes nothing: no coverage, no blend,
// no arithmetic. What it does is produce one record per visited texel in
// `zhao_surface_stamp`'s own j-outer/i-inner order, and a cursor that drifts
// from the consumer's writes every texel of a stamp to the wrong place while
// every handshake still looks healthy.
//
// So the engine is a MOCK here, and deliberately. A real `zhao_field_engine`
// would make each case a hundred times slower and would re-answer a question
// `field_engine_directed` already answers; what the mock adds is the ability to
// return a FAULT on demand, which is the case that matters most and which a
// loaded program cannot be made to produce on cue.
//
// The mock also RECORDS the (u, v) it was asked for, which is how the order is
// checked against the consumer's law rather than against this file's opinion of
// it -- a test that regenerated the expected order with the same expression the
// RTL uses would agree with itself.
//
//   1  a walk delivers exactly 4,096 records                     texels_o
//   2  ... in j-outer/i-inner order, u fast                      (the mock's log)
//   3  a stamp that did not ask for the brush produces nothing   stamps_o
//   4  a command arriving mid-walk RESTARTS the cursor           restarts_o
//   5  a faulting run still delivers its record                  faults_o
//   6  arm_ready_o follows residency

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_stamp_adapter.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

constexpr int kSheetW = 64;
constexpr int kSheetH = 64;
constexpr int kTexels = kSheetW * kSheetH;

using Dut = Vzhao_field_stamp_adapter;

void step(Dut& d) { zhao::tick(d); }

void reset(Dut& d) {
  d.rst_n = 0;
  d.cmd_fire_i = 0;
  d.cmd_field_en_i = 0;
  d.slot_i = 2;
  d.slot_valid_i = 1;
  d.fld_ready_i = 0;
  d.req_ready_i = 0;
  d.resp_valid_i = 0;
  d.resp_status_i = 0;
  for (int i = 0; i < 4; ++i) d.resp_out_i[i] = 0;
  d.eval();
  for (int i = 0; i < 3; ++i) step(d);
  d.rst_n = 1;
  step(d);
}

// A mock engine plus a mock consumer, driven together for `budget` cycles.
// Returns the (u, v) pairs the adapter asked for, in order.
struct Log {
  std::vector<uint32_t> u;
  std::vector<uint32_t> v;
  std::vector<uint32_t> tag_op;
  int records = 0;
};

// The mock engine, written to the SHARED engine's real protocol: one request in
// flight, the answer offered the cycle after the accept and HELD until the
// claimant takes it. Holding is the part that matters -- a mock that dropped an
// untaken answer would hide exactly the restart defect this file found, where
// abandoning an in-flight response parks the real engine forever.
//
// `fault_at` is an index into the request stream; -1 means never.
void pump(Dut& d, Log& log, int budget, int fault_at = -1) {
  for (int cycle = 0; cycle < budget; ++cycle) {
    d.eval();
    const bool can_accept = d.req_valid_o && !d.resp_valid_i;
    d.req_ready_i = can_accept ? 1 : 0;
    d.fld_ready_i = 1;
    d.eval();

    uint32_t u = 0;
    uint32_t v = 0;
    int index = -1;
    if (can_accept) {
      u = d.req_in_o[0];
      v = d.req_in_o[1];
      index = static_cast<int>(log.u.size());
      log.u.push_back(u);
      log.v.push_back(v);
    }
    const bool resp_taken = (d.resp_valid_i && d.resp_ready_o);
    if (d.fld_valid_o && d.fld_ready_i) {
      log.tag_op.push_back(d.fld_tag_op_o);
      log.records++;
    }

    step(d);

    if (resp_taken) d.resp_valid_i = 0;
    if (can_accept) {
      // The answer encodes the point it was asked about, so a record that
      // reaches the consumer can be traced back to the request that produced
      // it. A constant would make every record indistinguishable, which is
      // exactly the shape of the bug being hunted.
      d.resp_out_i[0] = (v << 16) | (u & 0xFFFF);
      d.resp_out_i[1] = 0x1234;
      d.resp_status_i = (index == fault_at) ? 0xF0 : 0x00;
      d.resp_valid_i = 1;
    }
  }
  d.req_ready_i = 0;
  d.fld_ready_i = 0;
}

void fire_command(Dut& d, bool field_en) {
  d.cmd_fire_i = 1;
  d.cmd_field_en_i = field_en ? 1 : 0;
  step(d);
  d.cmd_fire_i = 0;
  d.cmd_field_en_i = 0;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;
  reset(dut);

  // ---- 6. residency gates the arm ----------------------------------------
  dut.slot_valid_i = 0;
  dut.eval();
  check(dut.arm_ready_o == 0, "arm_ready_o low with no resident program", 0, dut.arm_ready_o);
  dut.slot_valid_i = 1;
  dut.eval();
  check(dut.arm_ready_o == 1, "arm_ready_o high once a program is resident", 1,
        dut.arm_ready_o);

  // ---- 3. a stamp that did not ask for the brush produces nothing ---------
  fire_command(dut, /*field_en=*/false);
  {
    Log quiet;
    pump(dut, quiet, 200);
    check(quiet.records == 0, "no records for a stamp that did not ask", 0,
          static_cast<uint64_t>(quiet.records));
    check(dut.stamps_o == 0, "stamps_o did not move", 0, dut.stamps_o);
    check(dut.busy_o == 0, "the adapter stayed idle", 0, dut.busy_o);
  }

  // ---- 1 and 2. a whole walk, in the consumer's order ---------------------
  fire_command(dut, /*field_en=*/true);
  check(dut.stamps_o == 1, "stamps_o fired", 1, dut.stamps_o);
  Log walk;
  pump(dut, walk, kTexels * 24);
  check(walk.records == kTexels, "exactly 4,096 records", kTexels,
        static_cast<uint64_t>(walk.records));
  check(dut.texels_o == static_cast<uint32_t>(kTexels), "texels_o counted them", kTexels,
        dut.texels_o);
  check(dut.busy_o == 0, "the walk ended", 0, dut.busy_o);

  // j-outer, i-inner: u runs 0..63 for each v, and v advances once per row.
  // Checked as a sequence rather than recomputed from the same expression the
  // RTL uses, because a test that re-derives the order agrees with itself.
  bool order_ok = (static_cast<int>(walk.u.size()) == kTexels);
  if (order_ok) {
    for (int t = 0; t < kTexels; ++t) {
      if (walk.u[t] != static_cast<uint32_t>(t % kSheetW) ||
          walk.v[t] != static_cast<uint32_t>(t / kSheetW)) {
        order_ok = false;
        std::printf("  first order break at t=%d: u=%u v=%u\n", t, walk.u[t], walk.v[t]);
        break;
      }
    }
  }
  check(order_ok, "the walk is j-outer / i-inner, u fast", 1, order_ok ? 1 : 0);

  // Every record carries the point its own request asked about. A cursor that
  // slipped by one would pass the count and fail here.
  bool payload_ok = (static_cast<int>(walk.tag_op.size()) == kTexels);
  if (payload_ok) {
    for (int t = 0; t < kTexels; ++t) {
      const uint32_t want = (static_cast<uint32_t>(t / kSheetW) << 16) |
                            static_cast<uint32_t>(t % kSheetW);
      if (walk.tag_op[t] != want) {
        payload_ok = false;
        std::printf("  first payload break at t=%d: got 0x%08X want 0x%08X\n", t,
                    walk.tag_op[t], want);
        break;
      }
    }
  }
  check(payload_ok, "record t carries the answer to request t", 1, payload_ok ? 1 : 0);

  // ---- 4. a command mid-walk restarts the cursor --------------------------
  // S4: an ACQUIRE that overflows aborts a stamp before any write, so the
  // consumer can begin a NEW stamp having consumed none of the old one's
  // records. Keeping our place would then deliver texel N's record for texel 0
  // -- well formed, plausible, and wrong for the rest of the sheet.
  fire_command(dut, true);
  Log partial;
  pump(dut, partial, 400);
  check(partial.records > 0 && partial.records < kTexels, "a walk is in progress", 1,
        static_cast<uint64_t>(partial.records));
  const uint32_t restarts_before = dut.restarts_o;
  fire_command(dut, true);
  check(dut.restarts_o == restarts_before + 1, "restarts_o fired", restarts_before + 1,
        dut.restarts_o);
  Log after;
  pump(dut, after, 400);
  check(after.u.size() > 0 && after.u[0] == 0 && after.v[0] == 0,
        "the cursor restarted at (0,0)", 0,
        after.u.empty() ? 0xFFFFFFFFu : (after.v[0] << 16 | after.u[0]));

  // Drain the restarted walk so the counters below are read from an idle block.
  Log drain;
  pump(dut, drain, kTexels * 24);
  check(dut.busy_o == 0, "the restarted walk completed", 0, dut.busy_o);

  // ---- 5. a faulting run still delivers its record ------------------------
  // The consumer is committed to 4,096 records by the time the first one is
  // asked for. Withholding one hangs the stamp, which is the worse failure.
  check(dut.faults_o == 0, "faults_o starts at zero", 0, dut.faults_o);
  fire_command(dut, true);
  Log faulty;
  pump(dut, faulty, kTexels * 24, /*fault_at=*/7);
  check(dut.faults_o == 1, "faults_o fired exactly once", 1, dut.faults_o);
  check(faulty.records == kTexels, "the faulting walk still delivered every record",
        kTexels, static_cast<uint64_t>(faulty.records));
  check(faulty.tag_op.size() > 7 && faulty.tag_op[7] == 0,
        "the faulted record is ZEROED, and the counter is what says so", 0,
        faulty.tag_op.size() > 7 ? faulty.tag_op[7] : 0xFFFFFFFFu);

  return zhao::report_and_exit("field_stamp_adapter_directed");
}
