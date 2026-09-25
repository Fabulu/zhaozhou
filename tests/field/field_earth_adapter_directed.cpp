// field_earth_adapter_directed.cpp -- THE E PROFILE'S STREAM ADAPTER: does it
// build the ratified earth record, does it answer the consumer's lane in list
// order, and does every one of its counters DISCRIMINATE?
//
// The block is `zhao_field_earth_adapter`, entry I34's build item (c). It is
// driven STANDALONE with the field engine PLAYED here, so a case can hand it an
// exact height out-lane and check the answer word for word -- which the
// composed article cannot do without loading a program, and loading a program
// would mean choosing one.
//
// THE ORACLE IS `reference/src/zrender/terrain.cpp:compose_lattice`, and the
// uniform law is transcribed from it rather than from prose:
//
//     if (frame_tick < start_tick) continue
//     age   = min(frame_tick - start_tick, duration)
//     phase = (duration == 0) ? 1.0fx : (age*65536 + duration/2) / duration
//
// Cases:
//    1  the uniform bank: age SATURATES at duration, phase is the EXACT
//       rounded divide, duration == 0 is 1.0                         records_o
//    2  the input record is field-ir.md 7.1's earth record, lane for lane,
//       with the vertex latched at the consumer's own accept
//    3  a covered lane RUNS and all four out-lanes leave on named ports  runs_o
//    4  a section 9.1 MISS is answered zero with NO RUN     skipped_uncovered_o
//    5  a field that has not begun is answered zero with NO RUN     not_begun_o
//    6  an unresolved handle raises `req_noprog_o` and the ENGINE refuses,
//       which is where `noprog_o` moves from                          noprog_o
//    7  an alarm status is counted apart from a refusal               faults_o
//    8  a seventeenth record on one list is rejected at the TAIL tail_rejected_o
//    9  the lane shadow is SILENT across a correct three-lane vertex, and
//       FIRES on all THREE of its arms                             lane_desync_o
//   10  the cost is measured: a consumer waiting on an unanswered lane
//                                                                stall_cycles_o
//   11  THE CAPTURE LAW (directive 15.1): a vertex moved underneath a run in
//       flight does not reach the engine, and does not reach the answer
//
// CASE 9 IS THE ONE WORTH READING. `lane_desync_o` differences this module's
// `vtx_live` against the CONSUMER's `fld_ready_o`, and nothing in either block
// loads both -- which is the property CLAUDE.md's metadata-swap chapter says to
// establish before quoting a checker's silence.
//
// ARM (c) AND CASE 9d ARE NEW WITH THE SYNCHRONOUS UNIFORM BANK (packet
// EARTHRAM). The payload now arrives a clock after its address, which is the
// exact shape of the metadata-swap defect -- A's data with B's metadata, every
// other counter balancing. Arm (c) differences the address the payload was
// READ AT (`cap_a`, loaded in the reset-free RAM process) against the lane the
// module believes it is serving (`cur_lane`, loaded in the main process by the
// consumer's handshake). Two processes, two enables, neither loading both. The case asserts the CORRECT
// behaviour first (the guard stays at zero across a real multi-lane vertex) and
// then fires it deliberately, from the consumer's side, because a detector
// nobody has watched move is a claim and not a measurement (R95).
//
// EVERY COUNTER IN THIS BLOCK IS REACHED BY LEGAL STIMULUS. None of them needs
// a committed mutant, and that is a statement about this block's shape rather
// than a convenience: it has no guard whose state is unreachable while the
// design is correct.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_field_earth_adapter.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;
using Dut = Vzhao_field_earth_adapter;

// `zhao_field_host`'s statuses above the ratified field-ir set.
constexpr uint8_t kStOk = 0x00;
constexpr uint8_t kStNoProgram = 0xF0;
constexpr uint8_t kStAlarm = 0xF2;

// The adapter's parameters, as this test verilates them: the SHARED client
// pair, not the earth profile's own 12/4.
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
  // All four canonical Earth ordinals present, which is what every case before
  // 12 assumes and what a complete Earth program returns. Case 12 is the one
  // that changes it.
  d.resp_present_i = 0xF;
  for (int i = 0; i < kOutLanes; ++i) d.resp_out_i[i] = 0;
  d.ans_ready_i = 0;
  for (int i = 0; i < 4; ++i) step(d);
  d.rst_n = 1;
  step(d);
}

// THE ORACLE'S UNIFORMS, in C, from compose_lattice. This is the EXPECTATION
// side of every age/phase check below and it is written once.
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

// Offer one TerrainField record's uniform half and wait for the take. The
// adapter holds `rec_ready_o` low through its divide, so this loop is also the
// measurement of that cost.
int offer_record(Dut& d, uint32_t start_tick, uint32_t duration, const uint32_t par[8],
                 bool last) {
  d.rec_start_tick_i = start_tick;
  d.rec_duration_i = duration;
  for (int i = 0; i < 8; ++i) d.rec_params_i[i] = par[i];
  d.rec_last_i = last ? 1 : 0;
  d.rec_valid_i = 1;
  int clocks = 0;
  // Take happens on the clock where ready is high; keep the offer up until it
  // is, then run out the divide so the entry is banked before the next one.
  while (!d.rec_ready_o && clocks < 200) {
    step(d);
    ++clocks;
  }
  step(d);  // the take
  ++clocks;
  d.rec_valid_i = 0;
  while (!d.rec_ready_o && clocks < 200) {
    step(d);
    ++clocks;
  }
  return clocks;
}

// Bind list slot `idx` to a publication object, exactly as the field list's
// per-patch replay does. `patch_open_i` first, then one `add_fire_i` per entry.
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

// The consumer's vertex accept: exactly the cycle `zhao_terrain_patch` latches
// `held_wx`/`held_wz`, and the cycle it raises `busy`. `ans_ready_i` is that
// `busy`, so it goes up on the clock AFTER the accept, which is what the block
// under test expects and what makes `lane_desync_o` silent.
void take_vertex(Dut& d, int32_t wx, int32_t wz, int lanes) {
  d.vtx_fire_i = 1;
  d.vtx_wx_i = wx;
  d.vtx_wz_i = wz;
  d.lanes_i = (uint8_t)lanes;
  step(d);
  d.vtx_fire_i = 0;
  d.ans_ready_i = lanes > 0 ? 1 : 0;
}

// Serve ONE lane: wait for either an engine request or an immediate answer,
// play the engine if asked, and take the answer. Returns false on a timeout.
// `present` is directive 8.1's output_present_mask over the four canonical
// Earth ordinals, defaulted to all four so every case written before it
// existed keeps describing a COMPLETE record. Case 12 is the one that hands
// over a short one.
bool serve_lane(Dut& d, bool covers, const int32_t out[4], uint8_t status, bool* ran,
                int32_t* height, int* req_slot, int* req_noprog, uint32_t req_in[15],
                uint32_t present = 0xF) {
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
      // The engine answers after a few clocks, as a real one does.
      for (int k = 0; k < 3; ++k) {
        step(d);
        ++guard;
      }
      for (int i = 0; i < kOutLanes; ++i) d.resp_out_i[i] = 0;
      for (int i = 0; i < 4; ++i) d.resp_out_i[i] = (uint32_t)out[i];
      d.resp_status_i = status;
      d.resp_present_i = present;
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
  if (height) *height = (int32_t)d.height_o;
  return true;
}

// Retire the answer the consumer is holding. `ans_ready_i` is already high.
void take_answer(Dut& d) { step(d); }

void zero_par(uint32_t par[8]) {
  for (int i = 0; i < 8; ++i) par[i] = 0;
}

// ---------------------------------------------------------------------------
// 1 -- the uniform bank, against the oracle's own arithmetic
// ---------------------------------------------------------------------------
void case1_uniforms(Dut& d) {
  struct Row {
    uint32_t tick, start, dur;
    const char* what;
  } rows[] = {
      {1000, 1000, 600, "age 0 at the very first tick of the span"},
      {1300, 1000, 600, "age 300 of 600 -- phase exactly a half"},
      {1599, 1000, 600, "age 599, the last tick inside the span"},
      {5000, 1000, 600, "age SATURATES at duration; a lapsed field persists"},
      {1000, 1000, 0, "duration 0: phase is 1.0 and the divide is skipped"},
      {1000, 1000, 1, "duration 1: phase is 1.0 by the rounded divide itself"},
      {1100, 1000, 7, "a duration that is not a power of two"},
      {1000, 4000, 900, "NOT BEGUN: the field does not act at all"},
  };
  const int n = (int)(sizeof(rows) / sizeof(rows[0]));
  uint32_t par[8];
  zero_par(par);

  for (int r = 0; r < n; ++r) {
    reset(d);
    d.tick_i = rows[r].tick;
    // p0 carries the row index, so a bank that stored the wrong entry's
    // parameters shows up as a wrong p0 in case 2's lane map rather than as a
    // silently identical record.
    par[0] = 0xA5A5'0000u | (uint32_t)r;
    offer_record(d, rows[r].start, rows[r].dur, par, true);
    check(d.records_o == 1, "case 1: one record banked", 1, d.records_o);

    const Uniforms u = oracle_uniforms(rows[r].tick, rows[r].start, rows[r].dur);

    // Read the uniforms back the only way the block exposes them: through a
    // run. One lane, resident, covering.
    const int obj[1] = {3};
    const bool res[1] = {true};
    replay(d, obj, res, 1);
    take_vertex(d, 111, 222, 1);
    bool ran = false;
    uint32_t in[15] = {0};
    const int32_t out[4] = {0, 0, 0, 0};
    const bool ok = serve_lane(d, true, out, kStOk, &ran, nullptr, nullptr, nullptr, in);
    check(ok, "case 1: the lane is answered", 1, ok ? 1 : 0);

    if (u.begun) {
      check(ran, "case 1: a begun, covering, resident lane RUNS", 1, ran ? 1 : 0);
      check(in[2] == u.age, rows[r].what, u.age, in[2]);
      check(in[3] == u.phase, "case 1: phase is the oracle's exact rounded divide", u.phase,
            in[3]);
      check(in[4] == par[0], "case 1: p0 is this record's, from this record's bank slot",
            par[0], in[4]);
    } else {
      check(!ran, "case 1: a field that has not begun NEVER runs", 0, ran ? 1 : 0);
      check(d.not_begun_o == 1, "case 1: and is counted as not-begun", 1, d.not_begun_o);
      check((int32_t)d.height_o == 0, "case 1: its height is the oracle's additive zero", 0,
            (int32_t)d.height_o);
    }
    take_answer(d);
  }
}

// ---------------------------------------------------------------------------
// 2 -- the input record, lane for lane (field-ir.md 7.1 earth)
// ---------------------------------------------------------------------------
void case2_record(Dut& d) {
  reset(d);
  d.tick_i = 2000;
  uint32_t par[8];
  for (int i = 0; i < 8; ++i) par[i] = 0x1000'0000u * (uint32_t)(i + 1) + 0x0BADu;
  offer_record(d, 1500, 1000, par, true);

  const int obj[1] = {5};
  const bool res[1] = {true};
  replay(d, obj, res, 1);

  const int32_t wx = -123456;
  const int32_t wz = 987654;
  take_vertex(d, wx, wz, 1);

  bool ran = false;
  uint32_t in[15] = {0};
  int slot = -1;
  int noprog = -1;
  const int32_t out[4] = {0, 0, 0, 0};
  const bool ok = serve_lane(d, true, out, kStOk, &ran, nullptr, &slot, &noprog, in);
  check(ok && ran, "case 2: the lane runs", 1, (ok && ran) ? 1 : 0);

  const Uniforms u = oracle_uniforms(2000, 1500, 1000);
  check((int32_t)in[0] == wx, "case 2: lane 0 is the vertex world x", (uint64_t)(uint32_t)wx,
        in[0]);
  check((int32_t)in[1] == wz, "case 2: lane 1 is the vertex world z", (uint64_t)(uint32_t)wz,
        in[1]);
  check(in[2] == u.age, "case 2: lane 2 is age", u.age, in[2]);
  check(in[3] == u.phase, "case 2: lane 3 is phase", u.phase, in[3]);
  for (int i = 0; i < 8; ++i) {
    check(in[4 + i] == par[i], "case 2: lanes 4..11 are p0..p7 in declaration order",
          par[i], in[4 + i]);
  }
  // The shared pair is FIFTEEN wide and the earth record is twelve. The three
  // the profile does not have must be ZERO and not stale bus.
  for (int i = 12; i < kInLanes; ++i) {
    check(in[i] == 0, "case 2: lanes 12..14 are padded zero, not stale", 0, in[i]);
  }
  check(slot == 5, "case 2: req_slot_o is the object the handle resolved to", 5,
        (uint64_t)slot);
  check(noprog == 0, "case 2: a resident lane does not raise req_noprog_o", 0,
        (uint64_t)noprog);
  take_answer(d);
}

// ---------------------------------------------------------------------------
// 3 -- all four out-lanes leave on named ports, from ONE evaluation
// ---------------------------------------------------------------------------
void case3_four_lanes(Dut& d) {
  reset(d);
  d.tick_i = 100;
  uint32_t par[8];
  zero_par(par);
  offer_record(d, 0, 100, par, true);

  const int obj[1] = {1};
  const bool res[1] = {true};
  replay(d, obj, res, 1);
  take_vertex(d, 7, 9, 1);

  const int32_t out[4] = {-77777, 314159, (int32_t)0xDEADBEEF, 424242};
  bool ran = false;
  int32_t h = 0;
  const bool ok = serve_lane(d, true, out, kStOk, &ran, &h, nullptr, nullptr, nullptr);
  check(ok && ran, "case 3: the lane runs", 1, (ok && ran) ? 1 : 0);
  check(h == out[0], "case 3: height_o is out-lane 0", (uint64_t)(uint32_t)out[0],
        (uint64_t)(uint32_t)h);
  check((int32_t)d.velocity_o == out[1], "case 3: velocity_o is out-lane 1",
        (uint64_t)(uint32_t)out[1], (uint64_t)d.velocity_o);
  check(d.material_o == (uint32_t)out[2], "case 3: material_o is out-lane 2",
        (uint32_t)out[2], d.material_o);
  check((int32_t)d.nav_cost_o == out[3], "case 3: nav_cost_o is out-lane 3",
        (uint64_t)(uint32_t)out[3], (uint64_t)d.nav_cost_o);
  check(d.ans_field_o == 1, "case 3: the answer carries a real field", 1, d.ans_field_o);
  check(d.runs_o == 1, "case 3: runs_o moved", 1, d.runs_o);
  take_answer(d);
}

// ---------------------------------------------------------------------------
// 4/5/6/7 -- the four refusals, each counted APART
// ---------------------------------------------------------------------------
void case4567_refusals(Dut& d) {
  // 4: a section 9.1 MISS. Answered zero, NO RUN.
  {
    reset(d);
    d.tick_i = 500;
    uint32_t par[8];
    zero_par(par);
    offer_record(d, 0, 100, par, true);
    const int obj[1] = {2};
    const bool res[1] = {true};
    replay(d, obj, res, 1);
    take_vertex(d, 1, 1, 1);
    bool ran = false;
    int32_t h = 1;
    const int32_t out[4] = {99, 99, 99, 99};
    const bool ok = serve_lane(d, /*covers=*/false, out, kStOk, &ran, &h, nullptr, nullptr,
                               nullptr);
    check(ok, "case 4: an uncovered lane is answered", 1, ok ? 1 : 0);
    check(!ran, "case 4: and NOT run -- section 9.1 is the whole saving here", 0,
          ran ? 1 : 0);
    check(h == 0, "case 4: its height is the additive zero", 0, (uint64_t)(uint32_t)h);
    check(d.ans_field_o == 0, "case 4: and the answer says it carries no field", 0,
          d.ans_field_o);
    check(d.skipped_uncovered_o == 1, "case 4: skipped_uncovered_o moved", 1,
          d.skipped_uncovered_o);
    check(d.runs_o == 0, "case 4: and runs_o did NOT", 0, d.runs_o);
    take_answer(d);
  }

  // 5: not begun. Covered by case 1's last row; asserted here beside its
  // siblings so the four causes are visibly separate counters.
  {
    reset(d);
    d.tick_i = 10;
    uint32_t par[8];
    zero_par(par);
    offer_record(d, 900, 100, par, true);
    const int obj[1] = {2};
    const bool res[1] = {true};
    replay(d, obj, res, 1);
    take_vertex(d, 1, 1, 1);
    bool ran = false;
    const int32_t out[4] = {99, 99, 99, 99};
    serve_lane(d, true, out, kStOk, &ran, nullptr, nullptr, nullptr, nullptr);
    check(d.not_begun_o == 1, "case 5: not_begun_o moved", 1, d.not_begun_o);
    check(d.skipped_uncovered_o == 0, "case 5: and the section 9.1 counter did NOT", 0,
          d.skipped_uncovered_o);
    take_answer(d);
  }

  // 6: the handle resolved to NO ready publication object. The lane is offered
  // to the engine with `req_noprog_o` RAISED -- the refusal is the engine's, so
  // `noprog_o` has exactly one place it can move from.
  {
    reset(d);
    d.tick_i = 500;
    uint32_t par[8];
    zero_par(par);
    offer_record(d, 0, 100, par, true);
    const int obj[1] = {0};
    const bool res[1] = {false};
    replay(d, obj, res, 1);
    take_vertex(d, 1, 1, 1);
    bool ran = false;
    int noprog = -1;
    int32_t h = 1;
    const int32_t out[4] = {99, 99, 99, 99};
    serve_lane(d, true, out, kStNoProgram, &ran, &h, nullptr, &noprog, nullptr);
    check(ran, "case 6: an unresolved lane IS offered to the engine", 1, ran ? 1 : 0);
    check(noprog == 1, "case 6: with req_noprog_o raised -- not a constant zero port", 1,
          (uint64_t)noprog);
    check(d.noprog_o == 1, "case 6: noprog_o moved", 1, d.noprog_o);
    check(d.faults_o == 0, "case 6: and faults_o did NOT -- the two are separable", 0,
          d.faults_o);
    check(h == 0, "case 6: a refused run contributes the additive zero", 0,
          (uint64_t)(uint32_t)h);
    take_answer(d);
  }

  // 7: an ALARM. Counted apart from a refusal.
  {
    reset(d);
    d.tick_i = 500;
    uint32_t par[8];
    zero_par(par);
    offer_record(d, 0, 100, par, true);
    const int obj[1] = {4};
    const bool res[1] = {true};
    replay(d, obj, res, 1);
    take_vertex(d, 1, 1, 1);
    bool ran = false;
    int32_t h = 1;
    const int32_t out[4] = {99, 99, 99, 99};
    serve_lane(d, true, out, kStAlarm, &ran, &h, nullptr, nullptr, nullptr);
    check(d.faults_o == 1, "case 7: faults_o moved", 1, d.faults_o);
    check(d.noprog_o == 0, "case 7: and noprog_o did NOT", 0, d.noprog_o);
    check(d.runs_o == 0, "case 7: an alarmed run is not a run that returned a value", 0,
          d.runs_o);
    check(h == 0, "case 7: and it contributes the additive zero", 0, (uint64_t)(uint32_t)h);
    take_answer(d);
  }
}

// ---------------------------------------------------------------------------
// 8 -- section 9.1 law 2: append in command order, reject the TAIL
// ---------------------------------------------------------------------------
void case8_tail_reject(Dut& d) {
  reset(d);
  d.tick_i = 1000;
  uint32_t par[8];
  zero_par(par);
  for (int i = 0; i < 17; ++i) {
    par[0] = (uint32_t)(0x7700'0000u + i);
    offer_record(d, 0, 100, par, i == 16);
  }
  check(d.records_o == 16, "case 8: sixteen records banked, the section 9.1 bound", 16,
        d.records_o);
  check(d.tail_rejected_o == 1, "case 8: the seventeenth is rejected at the TAIL", 1,
        d.tail_rejected_o);

  // And the rejected record is the SEVENTEENTH, not the first: entry 0 still
  // holds record 0's parameters. A bank that evicted instead of rejecting would
  // pass the two counts above and fail this.
  const int obj[1] = {6};
  const bool res[1] = {true};
  replay(d, obj, res, 1);
  take_vertex(d, 0, 0, 1);
  bool ran = false;
  uint32_t in[15] = {0};
  const int32_t out[4] = {0, 0, 0, 0};
  serve_lane(d, true, out, kStOk, &ran, nullptr, nullptr, nullptr, in);
  check(in[4] == 0x7700'0000u, "case 8: entry 0 is record 0 -- the TAIL was rejected, never evicted",
        0x7700'0000u, in[4]);
  take_answer(d);
}

// ---------------------------------------------------------------------------
// 9 -- THE SHADOW GUARD: silent when right, and shown to FIRE
// ---------------------------------------------------------------------------
void case9_shadow(Dut& d) {
  // (a) SILENT across a correct three-lane vertex.
  reset(d);
  d.tick_i = 1000;
  uint32_t par[8];
  zero_par(par);
  for (int i = 0; i < 3; ++i) {
    par[0] = (uint32_t)(0x3300'0000u + i);
    offer_record(d, 0, 100, par, i == 2);
  }
  const int obj[3] = {1, 2, 3};
  const bool res[3] = {true, true, true};
  replay(d, obj, res, 3);
  take_vertex(d, 42, 43, 3);
  for (int l = 0; l < 3; ++l) {
    bool ran = false;
    uint32_t in[15] = {0};
    const int32_t out[4] = {1000 * (l + 1), 0, 0, 0};
    int slot = -1;
    const bool ok = serve_lane(d, true, out, kStOk, &ran, nullptr, &slot, nullptr, in);
    check(ok && ran, "case 9a: every lane of the vertex runs", 1, (ok && ran) ? 1 : 0);
    check((uint32_t)slot == (uint32_t)obj[l],
          "case 9a: lanes are answered in LIST ORDER, with each entry's own slot",
          (uint64_t)obj[l], (uint64_t)slot);
    check(in[4] == (uint32_t)(0x3300'0000u + l),
          "case 9a: and with each entry's own uniforms", 0x3300'0000u + (uint32_t)l, in[4]);
    take_answer(d);
  }
  // The consumer drops `busy` on the last lane's handshake, exactly as
  // `zhao_terrain_patch` does.
  d.ans_ready_i = 0;
  step(d);
  step(d);
  check(d.lane_desync_o == 0, "case 9a: the shadow guard is SILENT when the shadow is right",
        0, d.lane_desync_o);
  check(d.idle_o == 1, "case 9a: and the block is idle again", 1, d.idle_o);

  // (b) FIRED. The consumer holds `fld_ready_o` high for a vertex this module
  // believes is finished -- which is exactly the fault the guard exists for,
  // and which no amount of correct adapter logic can produce on its own. The
  // guard's two operands are in two different blocks, so only the consumer's
  // side can create the disagreement, and that is the point.
  const uint32_t before = d.lane_desync_o;
  d.ans_ready_i = 1;
  step(d);
  check(d.lane_desync_o > before, "case 9b: the shadow guard FIRES on a consumer that disagrees",
        1, d.lane_desync_o > before ? 1 : 0);
  d.ans_ready_i = 0;

  // (c) The SECOND arm: a vertex accepted with a lane count this module never
  // saw replayed. `zhao_terrain_fieldlist` holds the vertex lane until its
  // replay is done, so this cannot happen while that interlock holds -- and
  // this is the check that would say so if it stopped.
  reset(d);
  d.tick_i = 1000;
  zero_par(par);
  offer_record(d, 0, 100, par, true);
  const int obj1[1] = {1};
  const bool res1[1] = {true};
  replay(d, obj1, res1, 1);
  const uint32_t before2 = d.lane_desync_o;
  d.vtx_fire_i = 1;
  d.vtx_wx_i = 0;
  d.vtx_wz_i = 0;
  d.lanes_i = 3;  // the consumer claims three; one was replayed
  step(d);
  d.vtx_fire_i = 0;
  d.ans_ready_i = 0;
  check(d.lane_desync_o > before2,
        "case 9c: the replay-count arm FIRES when the consumer's list and this bank disagree",
        1, d.lane_desync_o > before2 ? 1 : 0);

  // (d) THE THIRD ARM: the bank pairing. A consumer that accepts a new vertex
  // while a request is still in flight resets `cur_lane` underneath a payload
  // that was read at the OLD lane -- which is precisely "A's data with B's
  // metadata", and precisely what no other counter in this block looks at.
  //
  // THE OTHER TWO ARMS ARE HELD SILENT ACROSS THIS STIMULUS ON PURPOSE, so the
  // increment is attributable. Arm (a) differences `vtx_live` against
  // `ans_ready_i`: both are high throughout. Arm (b) fires on `vtx_fire_i`
  // with `rep_idx != lanes_i`: two entries are replayed and the vertex claims
  // two, so it is silent. A control pass with the same stimulus MINUS the
  // mid-flight accept is measured first, and it must leave the counter still.
  reset(d);
  d.tick_i = 1000;
  zero_par(par);
  offer_record(d, 0, 100, par, false);
  offer_record(d, 0, 100, par, true);
  const int obj2[2] = {1, 2};
  const bool res2[2] = {true, true};
  replay(d, obj2, res2, 2);

  // LANE 0 IS SERVED IN FULL FIRST, AND THAT IS LOAD-BEARING RATHER THAN
  // SETUP. The first version of this case fired the vertex while `cur_lane`
  // was still 0, so `cap_a` and `lane_a` were BOTH 0 and there was nothing for
  // the arm to disagree about -- the counter stayed still and read like a
  // detector that cannot fire. It is the stimulus that has to reach the state,
  // which is CLAUDE.md's "a gate that cannot reach the state is not evidence
  // about the state" arriving from the test's side.
  take_vertex(d, 0x0001'0000, 0x0002'0000, 2);
  const int32_t out9[4] = {0x0000'1234, 0, 0, 0};
  bool ran9 = false;
  int32_t h9 = 0;
  const bool ok9 = serve_lane(d, true, out9, 0x00, &ran9, &h9, nullptr, nullptr, nullptr);
  check(ok9, "case 9d: lane 0 is served first so the lane counter is non-zero", 1,
        ok9 ? 1 : 0);
  take_answer(d);  // cur_lane is now 1

  // -- the CONTROL: reach an in-flight request and do NOT disturb it ---------
  d.lane_covers_i = 1;
  int g = 0;
  while (!d.req_valid_o && g < 200) {
    step(d);
    ++g;
  }
  check(d.req_valid_o == 1, "case 9d: a request is in flight", 1, d.req_valid_o);
  const uint32_t quiet = d.lane_desync_o;
  step(d);
  step(d);
  check(d.lane_desync_o == quiet,
        "case 9d control: the pairing arm is SILENT while a request sits in flight",
        quiet, d.lane_desync_o);

  // -- FIRED: move the consumer's lane counter under the live request --------
  const uint32_t before3 = d.lane_desync_o;
  d.vtx_fire_i = 1;
  d.vtx_wx_i = 0x0003'0000;
  d.vtx_wz_i = 0x0004'0000;
  d.lanes_i = 2;  // equal to the replayed count, so arm (b) stays silent
  step(d);
  d.vtx_fire_i = 0;
  step(d);
  check(d.lane_desync_o > before3,
        "case 9d: the pairing arm FIRES when the lane moves under a live bank read",
        1, d.lane_desync_o > before3 ? 1 : 0);
  d.ans_ready_i = 0;
}

// ---------------------------------------------------------------------------
// 10 -- THE COST, measured
// ---------------------------------------------------------------------------
void case10_cost(Dut& d) {
  reset(d);
  d.tick_i = 1000;
  uint32_t par[8];
  zero_par(par);
  offer_record(d, 0, 100, par, true);
  const int obj[1] = {1};
  const bool res[1] = {true};
  replay(d, obj, res, 1);
  check(d.stall_cycles_o == 0, "case 10: no consumer, no stall", 0, d.stall_cycles_o);

  // COVERED, so the lane takes the engine path -- which is the case the cost
  // is about. An uncovered lane is answered in one clock and measures nothing.
  d.lane_covers_i = 1;
  take_vertex(d, 5, 5, 1);
  // The consumer is now holding `fld_ready_o` high with no answer, and the
  // engine is not accepting. This is exactly the scalar front's cost shape.
  for (int i = 0; i < 20; ++i) step(d);
  check(d.stall_cycles_o >= 20,
        "case 10: stall_cycles_o counts every clock the consumer waits -- THE COST", 20,
        d.stall_cycles_o);
  bool ran = false;
  const int32_t out[4] = {0, 0, 0, 0};
  serve_lane(d, true, out, kStOk, &ran, nullptr, nullptr, nullptr, nullptr);
  take_answer(d);
  d.ans_ready_i = 0;
  const uint32_t frozen = d.stall_cycles_o;
  for (int i = 0; i < 10; ++i) step(d);
  check(d.stall_cycles_o == frozen,
        "case 10: and does NOT count clocks the consumer is not waiting for one", frozen,
        d.stall_cycles_o);
}

// ---------------------------------------------------------------------------
// 11 -- directive 15.1's CAPTURE: the operands cannot move under a live run
// ---------------------------------------------------------------------------
void case11_capture(Dut& d) {
  reset(d);
  d.tick_i = 1000;
  uint32_t par[8];
  zero_par(par);
  par[0] = 0xC0DE'0001u;
  offer_record(d, 0, 100, par, true);
  const int obj[1] = {2};
  const bool res[1] = {true};
  replay(d, obj, res, 1);

  const int32_t wx = 0x0001'2345;
  const int32_t wz = 0x0006'7890;
  take_vertex(d, wx, wz, 1);
  d.lane_covers_i = 1;

  // Wait for the request to be offered, then MOVE the live vertex pins before
  // the engine accepts. A block that drove `req_in_o` from the live wires would
  // hand the engine the new point; this one drove it from the latch.
  int guard = 0;
  while (!d.req_valid_o && guard < 100) {
    step(d);
    ++guard;
  }
  check(d.req_valid_o == 1, "case 11: a request is offered", 1, d.req_valid_o);
  d.vtx_wx_i = 0x7FFF'FFFF;
  d.vtx_wz_i = (int32_t)0x8000'0000;
  step(d);
  check((int32_t)d.req_in_o[0] == wx,
        "case 11: req_in_o lane 0 is the LATCHED vertex, not the live pin",
        (uint64_t)(uint32_t)wx, d.req_in_o[0]);
  check((int32_t)d.req_in_o[1] == wz,
        "case 11: req_in_o lane 1 is the LATCHED vertex, not the live pin",
        (uint64_t)(uint32_t)wz, d.req_in_o[1]);

  d.req_ready_i = 1;
  step(d);
  d.req_ready_i = 0;
  for (int i = 0; i < kOutLanes; ++i) d.resp_out_i[i] = 0;
  d.resp_out_i[0] = 0x0000'4321u;
  d.resp_status_i = kStOk;
  d.resp_valid_i = 1;
  step(d);
  d.resp_valid_i = 0;
  guard = 0;
  while (!d.ans_valid_o && guard < 50) {
    step(d);
    ++guard;
  }
  check(d.ans_valid_o == 1, "case 11: and the answer still arrives", 1, d.ans_valid_o);
  check(d.height_o == 0x0000'4321u, "case 11: carrying the engine's out-lane 0", 0x4321u,
        d.height_o);
  take_answer(d);
}

// ---------------------------------------------------------------------------
// 12 -- R168/W10 ON THIS RECORD: A SHORT RECORD IS NOT A ZERO RESULT
// ---------------------------------------------------------------------------
// THE DEFECT THIS FIRES ON, stated so the case cannot be read as a feature
// test. Until 2026-09-23 this adapter gated its answer on `resp_status_i`
// ALONE. `zhao_field_host_v2` retires StOk when every ordinal the program's
// HEADER declared has landed -- not when this profile's four have -- so a
// two-ordinal Earth program returns 8'h00 with ordinals 2 and 3 clear, and the
// words behind them are the zero the host cleared at grant. The old adapter
// published those holes as VALUES, which is exactly the conflation owner
// directive 13.3 forbids: "a genuinely absent optional lane under an explicit
// compatible program signature is not a write of zero."
//
// BOTH HALVES ARE MEASURED HERE, and the second is the one that makes the
// first mean anything. The SHORT record must fire `short_record_o`, declare
// ordinals 2/3 absent and publish them as zero; the COMPLETE record, on
// byte-identical stimulus but for the mask, must leave the counter still and
// carry all four words. A counter that moved on both would be measuring the
// run, not the record.
//
// WHY NO COMMITTED MUTANT IS OWED (R95): this state is reachable with LEGAL
// stimulus from the block's own boundary, because a short record is a legal
// response the host is specified to produce. The mutant discipline is for a
// guard no legal input can reach.
void case12_short_record(Dut& d) {
  const int32_t out[4] = {0x0000'1111, 0x0000'2222, 0x0000'3333, 0x0000'4444};

  // ---- the SHORT record: height and velocity only ------------------------
  reset(d);
  d.tick_i = 1000;
  uint32_t par[8];
  zero_par(par);
  offer_record(d, 0, 100, par, true);
  const int obj[1] = {3};
  const bool res[1] = {true};
  replay(d, obj, res, 1);

  take_vertex(d, 0x0001'0000, 0x0002'0000, 1);
  bool ran = false;
  int32_t h = 0;
  bool ok = serve_lane(d, true, out, 0x00, &ran, &h, nullptr, nullptr, nullptr, 0x3);
  check(ok && ran, "case 12: the short-record lane runs", 1, (ok && ran) ? 1 : 0);

  check(d.short_record_o == 1, "case 12: short_record_o FIRES on a StOk record missing ordinals",
        1, d.short_record_o);
  check(d.ans_present_o == 0x3, "case 12: ans_present_o declares ordinals 0 and 1 present only",
        0x3, d.ans_present_o);
  check(h == out[0], "case 12: the PRESENT height is the engine's out-lane 0",
        (uint64_t)(uint32_t)out[0], (uint64_t)(uint32_t)h);
  check((int32_t)d.velocity_o == out[1], "case 12: the PRESENT velocity is out-lane 1",
        (uint64_t)(uint32_t)out[1], (uint64_t)(uint32_t)d.velocity_o);
  check(d.material_o == 0u, "case 12: the ABSENT material is published zero, not the hole's word",
        0, d.material_o);
  check((int32_t)d.nav_cost_o == 0, "case 12: the ABSENT nav_cost is published zero", 0,
        (uint64_t)(uint32_t)d.nav_cost_o);
  check(d.runs_o == 1, "case 12: a short record is still a RUN", 1, d.runs_o);
  check(d.faults_o == 0, "case 12: and is NOT a fault -- 13.3 rules an absent lane legitimate", 0,
        d.faults_o);
  check(d.ans_field_o == 1, "case 12: a real evaluation happened", 1, d.ans_field_o);
  const uint32_t short_fired = d.short_record_o;
  const uint32_t short_present = d.ans_present_o;
  take_answer(d);

  // ---- THE NEGATIVE CONTROL: the same everything, a COMPLETE mask --------
  reset(d);
  d.tick_i = 1000;
  zero_par(par);
  offer_record(d, 0, 100, par, true);
  replay(d, obj, res, 1);

  take_vertex(d, 0x0001'0000, 0x0002'0000, 1);
  ran = false;
  h = 0;
  ok = serve_lane(d, true, out, 0x00, &ran, &h, nullptr, nullptr, nullptr, 0xF);
  check(ok && ran, "case 12 control: the complete-record lane runs", 1, (ok && ran) ? 1 : 0);
  check(d.short_record_o == 0,
        "case 12 control: short_record_o STAYS SILENT on a complete record -- the counter reads "
        "the mask and not the run",
        0, d.short_record_o);
  check(d.ans_present_o == 0xF, "case 12 control: ans_present_o declares all four present", 0xF,
        d.ans_present_o);
  check(d.material_o == (uint32_t)out[2],
        "case 12 control: a PRESENT material carries out-lane 2 -- so case 12's zero is the mask's "
        "doing and not a suppression this commit added everywhere",
        (uint64_t)(uint32_t)out[2], d.material_o);
  check((int32_t)d.nav_cost_o == out[3], "case 12 control: a PRESENT nav_cost carries out-lane 3",
        (uint64_t)(uint32_t)out[3], (uint64_t)(uint32_t)d.nav_cost_o);

  // THE COUNTER'S OWN EVIDENCE, PRINTED AND NOT ONLY ASSERTED. A passing check
  // is a pass; this is the number. R95 and CLAUDE.md's broken-instrument law
  // both ask that a new counter be SEEN to move and seen to DISCRIMINATE, and
  // a silent harness that prints only a total cannot show either. The two
  // halves differ in the response mask alone.
  std::printf(
      "[field_earth_adapter_directed] R168 short-record control: "
      "short mask 0x%X -> short_record_o=%u ans_present_o=0x%X | "
      "complete mask 0xF -> short_record_o=%u ans_present_o=0x%X\n",
      0x3u, short_fired, short_present, d.short_record_o, d.ans_present_o);
  take_answer(d);
}


// ---------------------------------------------------------------------------
// 13 -- THE INTAKE/REPLAY INTERLOCK (packet EARTHLOCK, 2026-09-25)
//
// `rec_ready_o` is the FIRST beat of the intake and the banks land eighteen
// clocks later, so for eighteen clocks the console believes a record is in
// while entry N is still the PREVIOUS frame's. Two different failures live in
// that window and this case asserts the CORRECT behaviour of both. Neither
// check asserts the bug: after the repair there is no race to miss, and both
// of these still describe what the block must do.
//
//  (a) THE LOUD ONE. A replay landing inside the window sets `b_res[N]`, and
//      the intake's clear must not wipe it. Asserted as: the entry is RESIDENT
//      and the engine RUNS with the replay's own object. Before the repair the
//      late clear at I_WR won and this read `noprog`.
//  (b) THE SILENT ONE, which moves no counter at all and is the more dangerous
//      half. A vertex evaluating entry N inside the window must not be handed
//      the PREVIOUS frame's age, phase and parameters. Asserted as: whatever
//      the block's timing, the uniforms the engine is given are THIS frame's.
//      `noprog_o` cannot see this, which is the whole reason it is here.
// ---------------------------------------------------------------------------

// Offer a record and return the moment it is TAKEN -- in the middle of the
// intake, with the divide still running and the banks not yet written. This is
// the window `offer_record` deliberately runs out; case 13 is the one that
// stops inside it.
void offer_record_take_only(Dut& d, uint32_t start_tick, uint32_t duration,
                            const uint32_t par[8], bool last) {
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
  step(d);  // THE TAKE. The console now believes the record is in.
  d.rec_valid_i = 0;
  d.rec_last_i = 0;
}

// Run the intake out to its end, so the banks are committed.
void finish_intake(Dut& d) {
  int guard = 0;
  while (!d.rec_ready_o && guard < 200) {
    step(d);
    ++guard;
  }
  step(d);
}

void case13_intake_replay_interlock(Dut& d) {
  uint32_t par[8];

  // ---- (a) the replay's resident flag SURVIVES an in-flight intake --------
  reset(d);
  // FRAME 1: one record, banked to completion, replayed resident. This is the
  // state entry 0 is in when frame 2's intake starts, and it is what makes the
  // window's stale read possible at all.
  d.tick_i = 1050;
  zero_par(par);
  par[0] = 0xAAAA0001u;
  offer_record(d, 1000, 100, par, true);
  {
    const int obj[1] = {3};
    const bool res[1] = {true};
    replay(d, obj, res, 1);
  }

  // FRAME 2: take the record and STOP INSIDE THE INTAKE, then replay entry 0
  // while the divide is still running. That is exactly the ordering
  // `tests/terrain/composepub_acceptance.cpp` presented to the console-shaped
  // chain -- a handshake followed about six clocks later by the section 9.1
  // replay.
  d.tick_i = 1090;
  zero_par(par);
  par[0] = 0xBBBB0002u;
  offer_record_take_only(d, 1000, 100, par, true);
  {
    const int obj[1] = {5};
    const bool res[1] = {true};
    replay(d, obj, res, 1);  // lands INSIDE the eighteen-clock window
  }
  finish_intake(d);

  take_vertex(d, 7, 9, 1);
  {
    bool ran = false;
    int req_slot = -1;
    int req_noprog = -1;
    uint32_t in[15] = {0};
    const int32_t out[4] = {0x1234, 0, 0, 0};
    const bool served = serve_lane(d, true, out, kStOk, &ran, nullptr, &req_slot, &req_noprog, in);
    // This one only says a request reached the engine, which is TRUE EVEN WITH
    // THE DEFECT -- the wiped flag still issues a run, it issues it with
    // `req_noprog_o` raised and the engine refuses it. That is why the
    // residency assertion is the next check and not this one. Keeping them
    // apart is the difference between "1,089 runs" and "1,089 runs that moved
    // nothing", which is the whole shape of the defect.
    check(served && ran, "case 13a: the vertex reaches the engine at all", 1,
          (served && ran) ? 1 : 0);
    check(req_noprog == 0,
          "case 13a: and it is offered to the engine WITH a program, not as a no-program refusal", 0,
          req_noprog);
    check(req_slot == 5, "case 13a: bound to the REPLAY's object, which is frame 2's own binding", 5,
          req_slot);
    take_answer(d);
  }
  check(d.noprog_o == 0,
        "case 13a: and `noprog_o` never moved -- the loud symptom of the wiped flag is absent", 0,
        d.noprog_o);

  // ---- (b) a vertex inside the window gets THIS frame's uniforms ----------
  // The silent ordering. `b_begun` and `b_uni` are written at I_WR and read by
  // the lane stream, so a vertex firing after the replay but before I_WR would
  // evaluate on the previous frame's age, phase and parameters with resident
  // already set -- a real engine run on stale uniforms that moves NO counter.
  reset(d);
  d.tick_i = 1050;
  zero_par(par);
  par[0] = 0xAAAA0001u;
  offer_record(d, 1000, 100, par, true);
  {
    const int obj[1] = {3};
    const bool res[1] = {true};
    replay(d, obj, res, 1);
  }
  const Uniforms u_stale = oracle_uniforms(1050, 1000, 100);

  // FRAME 2, with uniforms that DIFFER from frame 1's in every field that is
  // read: a different age, a different phase and a different p0. If any of the
  // three comes back as frame 1's, the engine ran on a stale bank.
  d.tick_i = 1090;
  zero_par(par);
  par[0] = 0xBBBB0002u;
  offer_record_take_only(d, 1000, 100, par, true);
  const Uniforms u_fresh = oracle_uniforms(1090, 1000, 100);
  check(u_fresh.age != u_stale.age,
        "case 13b control: the two frames' ages really do differ, so the check CAN fail", 1,
        (u_fresh.age != u_stale.age) ? 1 : 0);
  {
    const int obj[1] = {5};
    const bool res[1] = {true};
    replay(d, obj, res, 1);
  }
  // THE VERTEX FIRES INSIDE THE WINDOW -- before I_WR, with the divide still
  // running. No waiting for `rec_ready_o` here: that wait is the bug's hiding
  // place.
  take_vertex(d, 7, 9, 1);
  {
    bool ran = false;
    uint32_t in[15] = {0};
    const int32_t out[4] = {0x1234, 0, 0, 0};
    const bool served = serve_lane(d, true, out, kStOk, &ran, nullptr, nullptr, nullptr, in);
    check(served && ran, "case 13b: the vertex is answered by a real engine run", 1,
          (served && ran) ? 1 : 0);
    check(in[2] == u_fresh.age,
          "case 13b: the engine is handed THIS frame's age, never the previous frame's", u_fresh.age,
          in[2]);
    check(in[3] == u_fresh.phase, "case 13b: and this frame's phase", u_fresh.phase, in[3]);
    check(in[4] == 0xBBBB0002u, "case 13b: and this frame's parameters", 0xBBBB0002u, in[4]);
    take_answer(d);
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;

  case1_uniforms(dut);
  case2_record(dut);
  case3_four_lanes(dut);
  case4567_refusals(dut);
  case8_tail_reject(dut);
  case9_shadow(dut);
  case10_cost(dut);
  case11_capture(dut);
  case12_short_record(dut);
  case13_intake_replay_interlock(dut);

  dut.final();
  return zhao::report_and_exit("field_earth_adapter_directed");
}
