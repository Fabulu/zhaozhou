// field_earth_adapter_intake_race_control.cpp
// ===========================================================================
// THE POSITIVE CONTROL for packet EARTHLOCK's intake/replay interlock.
//
// **THE POLARITY IS INVERTED: THIS PROGRAM PASSES WHEN THE RACE IS OBSERVED.**
//
// WHY IT EXISTS. `zhao_field_earth_adapter`'s `rec_ready_o` is the FIRST beat
// of its intake and the banks used to land EIGHTEEN CLOCKS LATER at I_WR,
// while the joined handshake had already told the rest of the console that
// record N was in. EARTHLOCK repaired that. After the repair THERE IS NO RACE
// TO MISS, so a test asserting the race would pass only while the defect
// existed -- which is the test CLAUDE.md forbids by name ("Do not write a test
// that asserts the bug"). The assertion of CORRECT behaviour lives where it
// belongs, in `tests/field/field_earth_adapter_directed.cpp` case 13, and it
// is silent about whether the repair was ever needed. THIS file is the half
// that says it was.
//
// It drives `tests/mutants/zhao_field_earth_adapter_intake_race_mutant.sv` --
// the production block with the repair reverted in five marked hunks and the
// module renamed so no source list can elaborate it. The stimulus is case 13's,
// beat for beat, so the two files are measuring ONE ordering from two sides.
//
// WHAT IT DEMANDS TO SEE, and it fails if the mutant is clean:
//
//   (a) THE LOUD HALF. A replay landing inside the intake window has its
//       resident flag WIPED by the late clear: `req_noprog_o` comes back 1
//       where production gives 0, and `req_slot_o` comes back 0 where
//       production gives the replay's own object.
//   (b) THE SILENT HALF, which is the one that owes this file the most. A
//       vertex evaluating in the window is handed the PREVIOUS FRAME's age,
//       phase and parameters with resident already set -- a real engine run on
//       stale uniforms that moves NOT ONE COUNTER. `noprog_o` is silent on it;
//       both arms of the lane shadow difference entry COUNTS and the count is
//       right; arm (c) pairs the read's own address with itself. No instrument
//       in the tree could see it, which is why the only evidence it was ever
//       there is this committed mutant.
//
// It is evidence about the REPAIR, not about the design. Nothing ships it.
// ===========================================================================

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_field_earth_adapter_intake_race_mutant.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;
using Dut = Vzhao_field_earth_adapter_intake_race_mutant;

constexpr uint8_t kStOk = 0x00;
constexpr int kInLanes = 15;
constexpr int kOutLanes = 7;
constexpr uint32_t kPhaseOne = 0x0001'0000u;

void step(Dut& d) { zhao::tick(d); }

void reset(Dut& d) {
  d.rst_n = 0;
  d.tick_i = 0;
  d.rec_valid_i = 0;
  d.rec_start_tick_i = 0;
  d.rec_duration_i = 0;
  for (int i = 0; i < 8; ++i) d.rec_params_i[i] = 0;
  d.rec_last_i = 0;
  d.patch_open_i = 0;
  d.add_fire_i = 0;
  d.add_obj_i = 0;
  d.add_resident_i = 0;
  d.vtx_fire_i = 0;
  d.vtx_wx_i = 0;
  d.vtx_wz_i = 0;
  d.lanes_i = 0;
  d.lane_covers_i = 0;
  d.req_ready_i = 0;
  d.resp_valid_i = 0;
  d.resp_status_i = 0;
  d.resp_present_i = 0xF;
  for (int i = 0; i < kOutLanes; ++i) d.resp_out_i[i] = 0;
  d.ans_ready_i = 0;
  for (int i = 0; i < 4; ++i) step(d);
  d.rst_n = 1;
  step(d);
}

struct Uniforms {
  bool begun;
  uint32_t age;
  uint32_t phase;
};

Uniforms oracle_uniforms(uint32_t frame_tick, uint32_t start_tick, uint32_t duration) {
  Uniforms u{};
  u.begun = frame_tick >= start_tick;
  const uint64_t span = u.begun ? (uint64_t)frame_tick - start_tick : 0;
  u.age = (uint32_t)(span > duration ? duration : span);
  u.phase = duration == 0
                ? kPhaseOne
                : (uint32_t)(((uint64_t)u.age * 65536u + duration / 2u) / duration);
  return u;
}

void offer_record(Dut& d, uint32_t start_tick, uint32_t duration, const uint32_t par[8],
                  bool last) {
  d.rec_start_tick_i = start_tick;
  d.rec_duration_i = duration;
  for (int i = 0; i < 8; ++i) d.rec_params_i[i] = par[i];
  d.rec_last_i = last ? 1 : 0;
  d.rec_valid_i = 1;
  int guard = 0;
  while (!d.rec_ready_o && guard < 200) {
    step(d);
    ++guard;
  }
  step(d);
  d.rec_valid_i = 0;
  d.rec_last_i = 0;
  while (!d.rec_ready_o && guard < 400) {
    step(d);
    ++guard;
  }
}

// Stop INSIDE the intake -- the window the defect lives in.
void offer_record_take_only(Dut& d, uint32_t start_tick, uint32_t duration, const uint32_t par[8],
                            bool last) {
  d.rec_start_tick_i = start_tick;
  d.rec_duration_i = duration;
  for (int i = 0; i < 8; ++i) d.rec_params_i[i] = par[i];
  d.rec_last_i = last ? 1 : 0;
  d.rec_valid_i = 1;
  int guard = 0;
  while (!d.rec_ready_o && guard < 200) {
    step(d);
    ++guard;
  }
  step(d);
  d.rec_valid_i = 0;
  d.rec_last_i = 0;
}

void finish_intake(Dut& d) {
  int guard = 0;
  while (!d.rec_ready_o && guard < 200) {
    step(d);
    ++guard;
  }
  step(d);
}

void replay(Dut& d, const int obj[], const bool resident[], int n) {
  d.patch_open_i = 1;
  step(d);
  d.patch_open_i = 0;
  for (int i = 0; i < n; ++i) {
    d.add_fire_i = 1;
    d.add_obj_i = (uint8_t)obj[i];
    d.add_resident_i = resident[i] ? 1 : 0;
    step(d);
  }
  d.add_fire_i = 0;
  step(d);
}

void take_vertex(Dut& d, int32_t wx, int32_t wz, int lanes) {
  d.vtx_fire_i = 1;
  d.vtx_wx_i = wx;
  d.vtx_wz_i = wz;
  d.lanes_i = (uint8_t)lanes;
  step(d);
  d.vtx_fire_i = 0;
  d.ans_ready_i = lanes > 0 ? 1 : 0;
}

bool serve_lane(Dut& d, bool covers, const int32_t out[4], uint8_t status, bool* ran,
                int* req_slot, int* req_noprog, uint32_t req_in[15]) {
  *ran = false;
  d.lane_covers_i = covers ? 1 : 0;
  int guard = 0;
  while (!d.ans_valid_o && guard < 400) {
    if (d.req_valid_o) {
      *ran = true;
      if (req_slot) *req_slot = d.req_slot_o;
      if (req_noprog) *req_noprog = d.req_noprog_o;
      if (req_in) {
        for (int i = 0; i < kInLanes; ++i) req_in[i] = d.req_in_o[i];
      }
      d.req_ready_i = 1;
      step(d);
      ++guard;
      d.req_ready_i = 0;
      for (int k = 0; k < 3; ++k) {
        step(d);
        ++guard;
      }
      for (int i = 0; i < kOutLanes; ++i) d.resp_out_i[i] = 0;
      for (int i = 0; i < 4; ++i) d.resp_out_i[i] = (uint32_t)out[i];
      d.resp_status_i = status;
      d.resp_present_i = 0xF;
      d.resp_valid_i = 1;
      step(d);
      ++guard;
      d.resp_valid_i = 0;
      continue;
    }
    step(d);
    ++guard;
  }
  if (!d.ans_valid_o) return false;
  return true;
}

void zero_par(uint32_t par[8]) {
  for (int i = 0; i < 8; ++i) par[i] = 0;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;
  uint32_t par[8];

  // ---- (a) THE LOUD HALF: the late clear wipes the replay -----------------
  reset(dut);
  dut.tick_i = 1050;
  zero_par(par);
  par[0] = 0xAAAA'0001u;
  offer_record(dut, 1000, 100, par, true);
  {
    const int obj[1] = {3};
    const bool res[1] = {true};
    replay(dut, obj, res, 1);
  }

  dut.tick_i = 1090;
  zero_par(par);
  par[0] = 0xBBBB'0002u;
  offer_record_take_only(dut, 1000, 100, par, true);
  {
    const int obj[1] = {5};
    const bool res[1] = {true};
    replay(dut, obj, res, 1);  // lands INSIDE the eighteen-clock window
  }
  finish_intake(dut);

  take_vertex(dut, 7, 9, 1);
  {
    bool ran = false;
    int req_slot = -1;
    int req_noprog = -1;
    uint32_t in[15] = {0};
    const int32_t out[4] = {0x1234, 0, 0, 0};
    const bool served = serve_lane(dut, true, out, kStOk, &ran, &req_slot, &req_noprog, in);
    check(served && ran, "MUTANT control: the vertex reaches the engine", 1,
          (served && ran) ? 1 : 0);
    // INVERTED: production gives 0 here. The mutant must give 1.
    check(req_noprog == 1,
          "MUTANT control (a): the intake's late clear WIPED the replay's resident flag", 1,
          req_noprog);
    check(req_slot == 0, "MUTANT control (a): and wiped its object binding with it", 0, req_slot);
    step(dut);
  }

  // ---- (b) THE SILENT HALF: a run on the previous frame's uniforms --------
  reset(dut);
  dut.tick_i = 1050;
  zero_par(par);
  par[0] = 0xAAAA'0001u;
  offer_record(dut, 1000, 100, par, true);
  {
    const int obj[1] = {3};
    const bool res[1] = {true};
    replay(dut, obj, res, 1);
  }
  const Uniforms u_stale = oracle_uniforms(1050, 1000, 100);

  dut.tick_i = 1090;
  zero_par(par);
  par[0] = 0xBBBB'0002u;
  offer_record_take_only(dut, 1000, 100, par, true);
  {
    const int obj[1] = {5};
    const bool res[1] = {true};
    replay(dut, obj, res, 1);
  }
  take_vertex(dut, 7, 9, 1);
  {
    bool ran = false;
    uint32_t in[15] = {0};
    const int32_t out[4] = {0x1234, 0, 0, 0};
    const bool served = serve_lane(dut, true, out, kStOk, &ran, nullptr, nullptr, in);
    check(served && ran, "MUTANT control (b): the vertex is answered by a real engine run", 1,
          (served && ran) ? 1 : 0);
    // INVERTED: production hands over frame 2's uniforms. The mutant hands
    // over frame 1's -- and NOTHING COUNTS IT.
    check(in[2] == u_stale.age,
          "MUTANT control (b): the engine got the PREVIOUS frame's age", u_stale.age, in[2]);
    check(in[3] == u_stale.phase, "MUTANT control (b): and the previous frame's phase",
          u_stale.phase, in[3]);
    check(in[4] == 0xAAAA'0001u, "MUTANT control (b): and the previous frame's parameters",
          0xAAAA'0001u, in[4]);
    step(dut);
  }
  // THE POINT OF THE WHOLE FILE, stated as a check: the silent half moved no
  // instrument. If any of these had counted it, the defect would have been
  // visible and this mutant would not have been needed.
  check(dut.noprog_o == 0, "MUTANT control (b): `noprog_o` NEVER MOVED on the stale-uniform run",
        0, dut.noprog_o);
  check(dut.lane_desync_o == 0,
        "MUTANT control (b): nor did any arm of the lane shadow guard", 0, dut.lane_desync_o);
  check(dut.faults_o == 0, "MUTANT control (b): nor `faults_o`", 0, dut.faults_o);

  dut.final();
  return zhao::report_and_exit("field_earth_adapter_intake_race_control");
}
