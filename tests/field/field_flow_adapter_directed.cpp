// field_flow_adapter_directed.cpp -- THE F PROFILE'S STREAM ADAPTER: does it
// present the ratified flow record, apply owner ruling R40's mapping, and keep
// the answer tied to the record it was computed from?
//
// The block is `zhao_field_flow_adapter`, entry I5's closure. It is driven
// STANDALONE with the field engine PLAYED here, so a case can hand it an exact
// velocity output and check the acceleration byte for byte -- which the
// composed article cannot do without loading a program, and loading a program
// would mean choosing one.
//
//   1  the input record is field-ir.md 7.1's flow record, lane for lane
//   2  R40's mapping: sat_s11((v' - v) >> ACC_SHIFT)              samples_o
//   3  a velocity delta past s11 SATURATES, never wraps           saturations_o
//   4  no program armed: answered at once, sample ABSENT not zero bypassed_o
//   5  ST_NO_PROGRAM: answered, sample absent, counted apart      noprog_o
//   6  an alarm status is counted apart from a refusal            faults_o
//   7  the answer is retired by the record's ACCEPT, so a back-to-back
//      offer cannot be answered with the previous record's field  rec_changed_o
//
// Case 7 is the one worth reading. `rec_changed_o` differences a record
// captured at REQUEST time against the live wire, so its two operands are
// loaded by different enables -- the property CLAUDE.md's metadata-swap chapter
// says to check before quoting any join's silence. The case asserts the CORRECT
// behaviour (the guard stays at zero across a back-to-back stream), and the
// guard's own ability to fire is shown separately, by holding the adapter in
// its answer state while the record underneath it changes.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_field_flow_adapter.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;
using Dut = Vzhao_field_flow_adapter;

// `zhao_field_host`'s statuses above the ratified field-ir set.
constexpr uint8_t kStOk = 0x00;
constexpr uint8_t kStNoProgram = 0xF0;
constexpr uint8_t kStAlarm = 0xF2;

// The adapter's parameters, as this test verilates them.
constexpr int kInLanes = 13;
constexpr int kOutLanes = 7;
constexpr int kPosShift = 8;
constexpr int kVelShift = 8;
constexpr int kAccShift = 8;

void step(Dut& d) { zhao::tick(d); }

void reset(Dut& d) {
  d.rst_n = 0;
  d.rec_valid_i = 0;
  d.rec_take_i = 0;
  d.origin_x_i = 0;
  d.origin_y_i = 0;
  d.origin_z_i = 0;
  d.slot_i = 0;
  d.slot_valid_i = 0;
  d.req_ready_i = 0;
  d.resp_valid_i = 0;
  d.resp_status_i = 0;
  for (int i = 0; i < kOutLanes; ++i) d.resp_out_i[i] = 0;
  for (int i = 0; i < 4; ++i) d.par_i[i] = 0;
  for (int i = 0; i < 4; ++i) d.rec_i[i] = 0;
  for (int i = 0; i < 4; ++i) step(d);
  d.rst_n = 1;
  step(d);
}

// The particle128 layout, packed the way `zhao_part_record` unpacks it.
// DERIVED FROM THE CODEC, not from a spec paragraph: the codec is the single
// implementation and a second reading of the layout here is exactly the
// duplication the adapter avoids by instantiating it.
//   [17:0] px  [35:18] py  [53:36] pz  [64:54] vx  [75:65] vy  [86:76] vz
//   [96:87] age  [103:97] species  [109:104] size  [115:110] spin
//   [119:116] flags  [127:120] variation
void set_record(Dut& d, int32_t px, int32_t py, int32_t pz, int32_t vx, int32_t vy,
                int32_t vz, uint32_t age, uint8_t variation) {
  uint64_t lo = 0, hi = 0;
  auto put = [&](uint64_t value, int width, int lsb) {
    const uint64_t mask = (width == 64) ? ~0ull : ((1ull << width) - 1ull);
    const uint64_t v = value & mask;
    if (lsb < 64) {
      lo |= v << lsb;
      if (lsb + width > 64) hi |= v >> (64 - lsb);
    } else {
      hi |= v << (lsb - 64);
    }
  };
  put(static_cast<uint64_t>(px), 18, 0);
  put(static_cast<uint64_t>(py), 18, 18);
  put(static_cast<uint64_t>(pz), 18, 36);
  put(static_cast<uint64_t>(vx), 11, 54);
  put(static_cast<uint64_t>(vy), 11, 65);
  put(static_cast<uint64_t>(vz), 11, 76);
  put(age, 10, 87);
  put(0, 7, 97);
  put(0, 6, 104);
  put(0, 6, 110);
  put(0, 4, 116);
  put(variation, 8, 120);
  d.rec_i[0] = static_cast<uint32_t>(lo & 0xFFFFFFFFu);
  d.rec_i[1] = static_cast<uint32_t>(lo >> 32);
  d.rec_i[2] = static_cast<uint32_t>(hi & 0xFFFFFFFFu);
  d.rec_i[3] = static_cast<uint32_t>(hi >> 32);
}

int32_t req_lane(Dut& d, int lane) { return static_cast<int32_t>(d.req_in_o[lane]); }

// What one offered record produced.
struct Answer {
  bool answered = false;
  bool field_valid = false;
  int32_t ax = 0, ay = 0, az = 0;
  bool saw_request = false;
  int32_t lanes[kInLanes] = {0};
};

int32_t s11(uint32_t raw) {
  const int32_t v = static_cast<int32_t>(raw & 0x7FFu);
  return (v & 0x400) ? (v - 0x800) : v;
}

// Offer one record, play the engine with `status` and `out_v*`, take the answer.
Answer offer(Dut& d, uint8_t status, int32_t ovx, int32_t ovy, int32_t ovz,
             int budget = 400) {
  Answer a;
  d.rec_valid_i = 1;
  for (int i = 0; i < budget; ++i) {
    d.eval();
    if (d.req_valid_o && !a.saw_request) {
      a.saw_request = true;
      for (int l = 0; l < kInLanes; ++l) a.lanes[l] = req_lane(d, l);
    }
    const bool req_taken = d.req_valid_o && d.req_ready_i;
    if (d.ans_valid_o) {
      a.answered = true;
      a.field_valid = d.fld_valid_o != 0;
      a.ax = s11(d.fld_ax_o);
      a.ay = s11(d.fld_ay_o);
      a.az = s11(d.fld_az_o);
      // Retire the record the way the composer does: the accept pulse.
      d.rec_take_i = 1;
      step(d);
      d.rec_take_i = 0;
      d.rec_valid_i = 0;
      d.resp_valid_i = 0;
      return a;
    }
    d.req_ready_i = 1;
    step(d);
    if (req_taken) {
      d.resp_valid_i = 1;
      d.resp_status_i = status;
      d.resp_out_i[3] = static_cast<uint32_t>(ovx);
      d.resp_out_i[4] = static_cast<uint32_t>(ovy);
      d.resp_out_i[5] = static_cast<uint32_t>(ovz);
    }
  }
  d.rec_valid_i = 0;
  return a;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;
  reset(dut);

  // A population origin that is NOT zero, so a missing origin term is visible
  // rather than hidden by an identity.
  const int32_t kOx = 0x0012'0000, kOy = -0x0003'0000, kOz = 0x0100'0000;
  dut.origin_x_i = kOx;
  dut.origin_y_i = kOy;
  dut.origin_z_i = kOz;
  dut.par_i[0] = 0x1111'1111u;
  dut.par_i[1] = 0x2222'2222u;
  dut.par_i[2] = 0x3333'3333u;
  dut.par_i[3] = 0x4444'4444u;

  // ---- 1 and 2. THE RECORD AND R40'S MAPPING ------------------------------
  const int32_t px = 1000, py = -250, pz = 33;
  const int32_t vx = 7, vy = -9, vz = 3;
  const uint32_t age = 41;
  const uint8_t variation = 0xC3;
  set_record(dut, px, py, pz, vx, vy, vz, age, variation);
  dut.slot_i = 2;
  dut.slot_valid_i = 1;

  // The engine answers a velocity a known distance from the input's.
  const int32_t in_vx = vx << kVelShift;
  const int32_t in_vy = vy << kVelShift;
  const int32_t in_vz = vz << kVelShift;
  const int32_t out_vx = in_vx + (5 << kAccShift);
  const int32_t out_vy = in_vy - (12 << kAccShift);
  const int32_t out_vz = in_vz + 3;  // a sub-LSB delta: R40's shift floors it

  Answer a1 = offer(dut, kStOk, out_vx, out_vy, out_vz);
  check(a1.saw_request, "the adapter issued a request", 1, a1.saw_request ? 1 : 0);
  check(a1.answered, "the adapter answered", 1, a1.answered ? 1 : 0);

  // field-ir.md 7.1, lane for lane. The three positions carry the origin.
  check(a1.lanes[0] == kOx + (px << kPosShift), "R0 = px, world fx16",
        kOx + (px << kPosShift), a1.lanes[0]);
  check(a1.lanes[1] == kOy + (py << kPosShift), "R1 = py, world fx16",
        kOy + (py << kPosShift), a1.lanes[1]);
  check(a1.lanes[2] == kOz + (pz << kPosShift), "R2 = pz, world fx16",
        kOz + (pz << kPosShift), a1.lanes[2]);
  check(a1.lanes[3] == in_vx, "R3 = vx", in_vx, a1.lanes[3]);
  check(a1.lanes[4] == in_vy, "R4 = vy", in_vy, a1.lanes[4]);
  check(a1.lanes[5] == in_vz, "R5 = vz", in_vz, a1.lanes[5]);
  check(a1.lanes[6] == static_cast<int32_t>(age), "R6 = age", static_cast<int>(age),
        a1.lanes[6]);
  check(a1.lanes[7] == static_cast<int32_t>(variation),
        "R7 = seed, and R40 says the seed IS the variation byte", variation, a1.lanes[7]);
  check(a1.lanes[8] == 0x0001'0000, "R8 = dt, one tick in Q16.16", 0x00010000,
        a1.lanes[8]);
  check(a1.lanes[9] == 0x1111'1111, "R9 = p0", 0x11111111, a1.lanes[9]);
  check(a1.lanes[12] == 0x4444'4444, "R12 = p3", 0x44444444, a1.lanes[12]);

  // R40: acceleration = sat_s11((v' - v) >> 8).
  check(a1.field_valid, "a good run produces a SAMPLE", 1, a1.field_valid ? 1 : 0);
  check(a1.ax == 5, "ax = (v'x - vx) >> 8", 5, a1.ax);
  check(a1.ay == -12, "ay = (v'y - vy) >> 8", -12, a1.ay);
  check(a1.az == 0, "az floors a sub-LSB delta, as R40 writes it", 0, a1.az);
  check(dut.samples_o == 1, "samples_o fired", 1, dut.samples_o);
  check(dut.saturations_o == 0, "a small delta does not saturate", 0, dut.saturations_o);

  // ---- 3. SATURATION, never a wrap ----------------------------------------
  // A wrapped acceleration REVERSES a particle, which `zhao_part_update`'s
  // overflow table calls out as reading like a physics bug.
  Answer a2 = offer(dut, kStOk, in_vx + (4000 << kAccShift), in_vy - (4000 << kAccShift),
                    in_vz);
  check(a2.answered, "the saturating run answered", 1, a2.answered ? 1 : 0);
  check(a2.ax == 1023, "ax clamped at s11 max", 1023, a2.ax);
  check(a2.ay == -1024, "ay clamped at s11 min", -1024, a2.ay);
  check(dut.saturations_o == 1, "saturations_o FIRED", 1, dut.saturations_o);

  // ---- 5. ST_NO_PROGRAM is answered, absent, and counted APART ------------
  Answer a3 = offer(dut, kStNoProgram, 0, 0, 0);
  check(a3.answered, "a refused run is still ANSWERED, never hung", 1,
        a3.answered ? 1 : 0);
  check(!a3.field_valid,
        "and the sample is ABSENT -- a low valid removes the term, it does not add zero",
        0, a3.field_valid ? 1 : 0);
  check(a3.ax == 0 && a3.ay == 0 && a3.az == 0, "the held lanes are cleared", 1,
        (a3.ax == 0 && a3.ay == 0 && a3.az == 0) ? 1 : 0);
  check(dut.noprog_o == 1, "noprog_o FIRED", 1, dut.noprog_o);
  check(dut.faults_o == 0, "a refusal is not counted as a fault", 0, dut.faults_o);

  // ---- 6. an ALARM is counted apart from a refusal ------------------------
  Answer a4 = offer(dut, kStAlarm, in_vx + (9 << kAccShift), in_vy, in_vz);
  check(a4.answered, "an alarmed run is answered", 1, a4.answered ? 1 : 0);
  check(!a4.field_valid, "an alarmed run produces no sample", 0, a4.field_valid ? 1 : 0);
  check(dut.faults_o == 1, "faults_o FIRED", 1, dut.faults_o);
  check(dut.noprog_o == 1, "and the refusal count did not move", 1, dut.noprog_o);
  check(dut.samples_o == 2, "samples_o counted only the two good runs", 2,
        dut.samples_o);

  // ---- 4. NO PROGRAM ARMED: answered AT ONCE, with no run -----------------
  // The console's arm gate. This is not a refusal by the engine -- the engine
  // is never asked -- so it is counted apart from both of the above.
  const uint32_t runs_before = dut.samples_o + dut.noprog_o + dut.faults_o;
  dut.slot_valid_i = 0;
  Answer a5 = offer(dut, kStOk, 0, 0, 0);
  check(a5.answered, "an unarmed console answers immediately", 1, a5.answered ? 1 : 0);
  check(!a5.saw_request, "and the engine was never asked", 0, a5.saw_request ? 1 : 0);
  check(!a5.field_valid, "the sample is absent", 0, a5.field_valid ? 1 : 0);
  check(dut.bypassed_o == 1, "bypassed_o FIRED", 1, dut.bypassed_o);
  check(dut.samples_o + dut.noprog_o + dut.faults_o == runs_before,
        "and no run was counted", static_cast<int>(runs_before),
        static_cast<int>(dut.samples_o + dut.noprog_o + dut.faults_o));
  dut.slot_valid_i = 1;

  // ---- 7. THE IDENTITY GUARD ---------------------------------------------
  // (a) CORRECT BEHAVIOUR: a back-to-back stream, each record retired by its
  //     own accept, never trips the guard. This is the assertion that survives
  //     the repair; asserting the defect would pass only while it existed.
  check(dut.rec_changed_o == 0, "the guard is clean so far", 0, dut.rec_changed_o);
  for (int i = 0; i < 4; ++i) {
    set_record(dut, 100 + i, 200 + i, 300 + i, i, -i, i * 2, 10 + i,
               static_cast<uint8_t>(i));
    Answer ai = offer(dut, kStOk, (i << kVelShift) + (i << kAccShift), -(i << kVelShift),
                      (i * 2) << kVelShift);
    check(ai.answered, "back-to-back record answered", 1, ai.answered ? 1 : 0);
    check(ai.ax == i, "and it is THIS record's acceleration", i, ai.ax);
  }
  check(dut.rec_changed_o == 0,
        "the guard stayed at zero across a back-to-back stream -- every answer "
        "belonged to the record it was offered with",
        0, dut.rec_changed_o);

  // (b) THE GUARD CAN FIRE, shown by holding the adapter in its answer state
  //     while the record underneath it is replaced without an accept. That is
  //     not legal traffic -- the composer's accept is what retires an answer --
  //     and it is here so the zero above is a measurement rather than a claim.
  set_record(dut, 1, 2, 3, 4, 5, 6, 7, 8);
  dut.rec_valid_i = 1;
  bool reached_answer = false;
  for (int i = 0; i < 200 && !reached_answer; ++i) {
    dut.eval();
    const bool req_taken = dut.req_valid_o && dut.req_ready_i;
    if (dut.ans_valid_o) reached_answer = true;
    dut.req_ready_i = 1;
    step(dut);
    if (req_taken) {
      dut.resp_valid_i = 1;
      dut.resp_status_i = kStOk;
      dut.resp_out_i[3] = 0;
      dut.resp_out_i[4] = 0;
      dut.resp_out_i[5] = 0;
    }
  }
  check(reached_answer, "the adapter is holding an answer", 1, reached_answer ? 1 : 0);
  // Swap the record WITHOUT an accept. The guard must notice.
  set_record(dut, 99, 98, 97, 1, 1, 1, 3, 0x7F);
  for (int i = 0; i < 4; ++i) step(dut);
  check(dut.rec_changed_o > 0,
        "the identity guard FIRED when the record moved under a held answer -- "
        "its zero above is an instrument reading, not an argument",
        1, dut.rec_changed_o > 0 ? 1 : 0);

  // =========================================================================
  // 8. FT097 -- THE CAPTURE. R40's SUBTRAHEND IS THE RECORD THE RUN STARTED
  //    WITH, NOT WHATEVER IS ON THE PINS WHEN THE ANSWER ARRIVES.
  // =========================================================================
  // Owner directive 15.1: "Latch parameters, origin, dt, frame and the actual
  // particle record at request capture. DERIVE NO LATER RESULT FROM UNRELATED
  // LIVE rec_i OR par_i PINS."
  //
  // This is the case that executes the defect that sentence names. Until
  // 2026-09-20 the adapter read `in_vx` -- R40's SUBTRAHEND -- from the live
  // pins at RESPONSE time, so a record that moved mid-flight produced
  // `(v' of record A) - (v of record B)`.
  //
  // Record A is offered and its run is accepted. The record is then replaced
  // with B WHILE THE RUN IS IN FLIGHT, and the engine answers a velocity
  // chosen so that the two readings are far apart and both in range:
  //
  //     from the CAPTURED A :  (17<<8 - 10<<8)      >> 8  =    7   <- correct
  //     from the LIVE     B :  (17<<8 - (-500<<8))  >> 8  =  517   <- the defect
  //
  // Neither saturates, so this discriminates on the VALUE and not on a clamp.
  // The assertion is the CORRECT behaviour -- it keeps passing after the
  // repair, which a test that asserted the defect would not.
  {
    // Start from a quiet block.
    dut.rec_valid_i = 0;
    dut.resp_valid_i = 0;
    dut.req_ready_i = 0;
    for (int i = 0; i < 6; ++i) step(dut);

    const int32_t kVxA = 10;
    const int32_t kVxB = -500;
    const int32_t in_vxA = kVxA << kVelShift;
    const int32_t out_vx8 = in_vxA + (7 << kAccShift);

    const uint32_t changed_before = dut.rec_changed_o;

    set_record(dut, 5, 6, 7, kVxA, 0, 0, 12, 0x5A);
    dut.rec_valid_i = 1;

    bool accepted = false;
    bool swapped = false;
    bool answered = false;
    int32_t got_ax = 0x7FFF'FFFF;
    int32_t offered_p0_at_accept = 0;

    for (int i = 0; i < 400 && !answered; ++i) {
      dut.eval();
      if (dut.ans_valid_o) {
        answered = true;
        got_ax = s11(dut.fld_ax_o);
        break;
      }
      const bool req_taken = dut.req_valid_o && dut.req_ready_i;
      if (dut.req_valid_o && !accepted) offered_p0_at_accept = req_lane(dut, 9);
      dut.req_ready_i = 1;
      step(dut);
      if (req_taken && !accepted) {
        accepted = true;
        // THE RUN IS NOW IN FLIGHT. Move the record underneath it, exactly as
        // a producer that did not wait for the accept would.
        set_record(dut, 900, 901, 902, kVxB, 0, 0, 99, 0x0F);
        swapped = true;
        dut.resp_valid_i = 1;
        dut.resp_status_i = kStOk;
        dut.resp_out_i[3] = static_cast<uint32_t>(out_vx8);
        dut.resp_out_i[4] = 0;
        dut.resp_out_i[5] = 0;
      }
    }

    // `rec_changed_o` is a REGISTERED count sampled while the block sits in
    // F_ANS, so it cannot have moved at the first `eval` that shows
    // `ans_valid_o`: no clock edge has happened in that state yet. Give it
    // two, WITHOUT retiring -- `rec_valid_i` is still high and `rec_take_i` is
    // low, so the answer is still being held and record B is still offered.
    for (int i = 0; i < 2; ++i) step(dut);

    check(accepted && swapped, "8: the record was swapped while the run was in flight", 1,
          (accepted && swapped) ? 1 : 0);
    check(answered, "8: the adapter answered", 1, answered ? 1 : 0);
    check(got_ax == 7,
          "8 FT097: R40's subtrahend came from the CAPTURED record (7), not the live "
          "pins (517) -- the two sides of (v' - v) are the same particle",
          7, got_ax);
    check(got_ax != 517,
          "8 FT097: and it is specifically NOT the live-pin answer the defect gave", 1,
          (got_ax != 517) ? 1 : 0);
    // The guard still reports that the producer misbehaved. Both things are
    // true at once, and that is the point: the arithmetic is protected BY
    // CONSTRUCTION, and the counter is the REPORT rather than the mechanism.
    check(dut.rec_changed_o > changed_before,
          "8: rec_changed_o still FIRES on the swap -- the guard remains reachable after "
          "the repair, and it is a report, not the thing keeping the value right",
          1, (dut.rec_changed_o > changed_before) ? 1 : 0);
    // Lane 9 is p0, which is `par_i[0]` -- the value set at the top of main.
    // (Lane 12 is p3 = par_i[3] = 0x4444'4444; case 1 asserts that one.)
    check(offered_p0_at_accept == 0x1111'1111,
          "8: the offered p0 was the captured one", 0x11111111, offered_p0_at_accept);

    dut.rec_take_i = 1;
    step(dut);
    dut.rec_take_i = 0;
    dut.rec_valid_i = 0;
    dut.resp_valid_i = 0;
    dut.req_ready_i = 0;
    for (int i = 0; i < 4; ++i) step(dut);
  }

  // =========================================================================
  // 9. FT097 -- A STANDING OFFER'S OPERANDS CANNOT MOVE UNDERNEATH IT
  // =========================================================================
  // The other half of 15.1's capture rule, and the hazard
  // `zhao_field_stamp_adapter`'s S_DROP comment names from the other side:
  // "CHANGE AN OFFER THAT IS STILL STANDING ... resetting the counter under it
  // would move the operands of a request the engine has not yet accepted."
  //
  // `req_ready_i` is held LOW so the request stands, and `par_i` and the
  // record are both rewritten. The offered lanes must not move.
  {
    set_record(dut, 11, 12, 13, 4, 0, 0, 20, 0x11);
    dut.par_i[0] = 0xAAAA'0000u;
    dut.eval();
    dut.rec_valid_i = 1;
    dut.req_ready_i = 0;

    bool standing = false;
    int32_t p0_before = 0, r0_before = 0, r3_before = 0;
    for (int i = 0; i < 50 && !standing; ++i) {
      dut.eval();
      if (dut.req_valid_o) {
        standing = true;
        p0_before = req_lane(dut, 9);
        r0_before = req_lane(dut, 0);
        r3_before = req_lane(dut, 3);
        break;
      }
      step(dut);
    }
    check(standing, "9: an offer is standing, unaccepted", 1, standing ? 1 : 0);
    check(p0_before == static_cast<int32_t>(0xAAAA'0000u),
          "9: it carries the p0 that was live at capture", 0xAAAA0000, p0_before);

    // Move everything under the standing offer.
    dut.par_i[0] = 0xBBBB'1111u;
    set_record(dut, 777, 778, 779, -300, 0, 0, 55, 0x99);
    dut.eval();
    for (int i = 0; i < 3; ++i) { step(dut); dut.eval(); }

    check(dut.req_valid_o == 1, "9: the offer is still standing", 1, dut.req_valid_o);
    check(req_lane(dut, 9) == p0_before,
          "9 FT097: p0 did NOT move under the standing offer", p0_before, req_lane(dut, 9));
    check(req_lane(dut, 0) == r0_before,
          "9 FT097: nor the position lane", r0_before, req_lane(dut, 0));
    check(req_lane(dut, 3) == r3_before,
          "9 FT097: nor the velocity lane R40 will subtract", r3_before, req_lane(dut, 3));

    // Let it finish so the block is left idle.
    dut.req_ready_i = 1;
    for (int i = 0; i < 200; ++i) {
      dut.eval();
      const bool req_taken = dut.req_valid_o && dut.req_ready_i;
      if (dut.ans_valid_o) break;
      step(dut);
      if (req_taken) {
        dut.resp_valid_i = 1;
        dut.resp_status_i = kStOk;
        for (int l = 3; l < 6; ++l) dut.resp_out_i[l] = 0;
      }
    }
    dut.rec_take_i = 1;
    step(dut);
    dut.rec_take_i = 0;
    dut.rec_valid_i = 0;
    dut.resp_valid_i = 0;
    dut.req_ready_i = 0;
    for (int i = 0; i < 4; ++i) step(dut);
  }

  // =========================================================================
  // 10. THE NO-SECOND-INTEGRATOR RULE, ASSERTED RATHER THAN ASSUMED
  // =========================================================================
  // R40 maps this seam onto PART.UPDATE's ACCELERATION port, and PART.UPDATE
  // integrates position itself. Output ordinals 0/1/2 (px'/py'/pz') and 6
  // (attr0) must therefore change NOTHING here. A program that returned a wild
  // position and an unchanged velocity must produce acceleration zero -- if
  // the adapter ever grew a position path, this is the case that would catch
  // it, and it would catch it as a VALUE rather than as a review comment.
  {
    const uint32_t samples_before = dut.samples_o;
    set_record(dut, 3, 4, 5, 6, 7, 8, 30, 0x2B);
    const int32_t vin_x = 6 << kVelShift;
    const int32_t vin_y = 7 << kVelShift;
    const int32_t vin_z = 8 << kVelShift;

    dut.rec_valid_i = 1;
    bool answered = false;
    int32_t ax = 0x7FFF'FFFF, ay = 0x7FFF'FFFF, az = 0x7FFF'FFFF;
    for (int i = 0; i < 400 && !answered; ++i) {
      dut.eval();
      if (dut.ans_valid_o) {
        answered = true;
        ax = s11(dut.fld_ax_o);
        ay = s11(dut.fld_ay_o);
        az = s11(dut.fld_az_o);
        break;
      }
      const bool req_taken = dut.req_valid_o && dut.req_ready_i;
      dut.req_ready_i = 1;
      step(dut);
      if (req_taken) {
        dut.resp_valid_i = 1;
        dut.resp_status_i = kStOk;
        // Ordinals 0/1/2: a WILD new position. Ordinal 6: a wild attr0.
        dut.resp_out_i[0] = 0x7FFF'0000u;
        dut.resp_out_i[1] = 0x8000'1234u;
        dut.resp_out_i[2] = 0x0BAD'F00Du;
        // Ordinals 3/4/5: the velocity is UNCHANGED.
        dut.resp_out_i[3] = static_cast<uint32_t>(vin_x);
        dut.resp_out_i[4] = static_cast<uint32_t>(vin_y);
        dut.resp_out_i[5] = static_cast<uint32_t>(vin_z);
        dut.resp_out_i[6] = 0xFFFF'FFFFu;
      }
    }
    check(answered, "10: answered", 1, answered ? 1 : 0);
    check(ax == 0 && ay == 0 && az == 0,
          "10: an unchanged velocity gives ZERO acceleration however wild the returned "
          "position is -- there is no second integrator and attr0 is not read",
          0, (ax == 0 && ay == 0 && az == 0) ? 0 : 1);
    check(dut.samples_o == samples_before + 1,
          "10: and it was still counted as a real sample", samples_before + 1,
          dut.samples_o);
    dut.rec_take_i = 1;
    step(dut);
    dut.rec_take_i = 0;
    dut.rec_valid_i = 0;
    dut.resp_valid_i = 0;
    for (int i = 0; i < 4; ++i) step(dut);
  }

  return zhao::report_and_exit("field_flow_adapter_directed");
}
