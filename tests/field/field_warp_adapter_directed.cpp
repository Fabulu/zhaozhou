// field_warp_adapter_directed.cpp -- the W profile's stream adapter,
// GEOM.WARP prerequisite P2.
//
// THE QUESTION THIS FILE ANSWERS is the lane binding and the absent-versus-
// zero rule, because those are the two things this block can get wrong in a
// way that looks healthy.
//
// ===========================================================================
// THE CASE THAT MATTERS MOST: AN IDENTITY WARP AND A REFUSED RUN CARRY
// BYTE-IDENTICAL DATA
// ===========================================================================
// Decision W10: "Publish no partially warped meshlet ... AN ABSENT OUTPUT MUST
// NOT LOOK LIKE A ZERO RESULT." `GEOM.WARP.md:149-151` and decision W09 make
// an identity a first-class, successful result: "An identity Warp (d = 0,
// n_out = n_in) over a lawful upstream normal performs no additional shift and
// lighting is exact."
//
// So these two states have the SAME six output words -- all zero displacement:
//
//     IDENTITY : the program ran and chose not to move the vertex
//     REFUSED  : no program answered at all
//
// and they are distinguished ONLY by `warp_valid_o`. Case D drives both and
// asserts that the DATA is identical while the VALIDITY differs. That is the
// whole point: a consumer that inspected the displacement to decide whether a
// Warp happened would silently treat every refusal as an identity, and an
// identity looks exactly like a mesh that is fine -- the flattering direction.
//
// This is deliberately NOT a test that asserts a bug. It asserts the correct
// behaviour in BOTH polarities, which is the shape R111's case 1d established.
//
// ===========================================================================
// WHY THE ENGINE IS A MOCK
// ===========================================================================
// The same reason as the sibling adapters: checking the lane binding needs an
// exact, chosen response, and getting one from the real host would mean
// choosing a Warp program -- an ABI decision that does not belong in a test.
// `zhao_geom_warp.sv` does not exist at this commit, so the CONSUMER is
// modelled here too, and this file never pretends otherwise.
//
//   A  the 15-lane canonical input record, per lane
//   B  the 6 canonical output ordinals, passed through unchanged
//   C  FH27: a non-Warp program in the slot is refused ABOVE execution
//   D  W10: identity and refusal carry the same data and differ in validity
//   E  the metadata-swap guard fires when the vertex moves under the answer
//   F  the cost counter moves

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_field_warp_adapter.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

using Dut = Vzhao_field_warp_adapter;

constexpr uint8_t kWarpProfile = 1;   // field-ir.md 7.1: earth 0, warp 1, ...
constexpr uint8_t kFlowProfile = 2;

void step(Dut& d) { zhao::tick(d); }

// A distinguishable value per lane, so a swapped or dropped lane is visible.
// p3 is lane 14 -- the lane the "(14)" parenthetical used to drop, which
// field-ir.md:536-545 records as reading like "a plausible number from a lane
// nobody supplied". It gets a sentinel for exactly that reason.
constexpr uint32_t kPx = 0x1111'0001u;
constexpr uint32_t kPy = 0x1111'0002u;
constexpr uint32_t kPz = 0x1111'0003u;
constexpr uint32_t kNx = 0x2222'0001u;
constexpr uint32_t kNy = 0x2222'0002u;
constexpr uint32_t kNz = 0x2222'0003u;
constexpr uint32_t kA0 = 0x3333'0000u;
constexpr uint32_t kA1 = 0x3333'0001u;
constexpr uint32_t kA2 = 0x3333'0002u;
constexpr uint32_t kA3 = 0x3333'0003u;
constexpr uint32_t kTime = 0xDEAD'BEEFu;   // u32 tick DATA, bit-cast not converted
constexpr uint32_t kP0 = 0x4444'0000u;
constexpr uint32_t kP1 = 0x4444'0001u;
constexpr uint32_t kP2 = 0x4444'0002u;
// The p3 SENTINEL. Named rather than inline so the assertion below reads as
// "lane 14 carried THIS", not "lane 14 carried something". zref_geom_warp.hpp
// keeps a `P3_SENTINEL` fixture for the same reason: to make a dropped lane 14
// change an observable value instead of quietly reading a cleared register.
constexpr uint32_t kP3 = 0x4444'0003u;

void present_vertex(Dut& d) {
  d.px_i = kPx;  d.py_i = kPy;  d.pz_i = kPz;
  d.nx_i = kNx;  d.ny_i = kNy;  d.nz_i = kNz;
  d.attr_i[0] = kA0;  d.attr_i[1] = kA1;  d.attr_i[2] = kA2;  d.attr_i[3] = kA3;
  d.time_i = kTime;
  d.par_i[0] = kP0;  d.par_i[1] = kP1;  d.par_i[2] = kP2;  d.par_i[3] = kP3;
}

void reset(Dut& d) {
  d.rst_n = 0;
  d.vtx_valid_i = 0;
  d.vtx_take_i = 0;
  d.slot_i = 3;
  d.slot_valid_i = 1;
  d.prog_profile_i = kWarpProfile;
  d.req_ready_i = 0;
  d.resp_valid_i = 0;
  d.resp_status_i = 0;
  for (int i = 0; i < 7; ++i) d.resp_out_i[i] = 0;
  present_vertex(d);
  d.eval();
  for (int i = 0; i < 3; ++i) step(d);
  d.rst_n = 1;
  step(d);
}

// What the adapter offered the engine, captured at the accept.
struct Offer {
  uint32_t lane[15] = {0};
  bool seen = false;
};

// One vertex through the adapter. Returns the offer (if any) and leaves the
// DUT parked in its answer state with `ans_valid_o` high, so the caller can
// inspect the result before retiring it.
Offer run_vertex(Dut& d, const uint32_t out[6], uint8_t status, int budget = 40) {
  Offer o;
  d.vtx_valid_i = 1;
  d.eval();
  for (int cycle = 0; cycle < budget; ++cycle) {
    d.eval();
    if (d.ans_valid_o) break;
    const bool can_accept = d.req_valid_o && !d.resp_valid_i;
    d.req_ready_i = can_accept ? 1 : 0;
    d.eval();
    if (can_accept) {
      for (int i = 0; i < 15; ++i) o.lane[i] = d.req_in_o[i];
      o.seen = true;
    }
    const bool resp_taken = d.resp_valid_i && d.resp_ready_o;
    step(d);
    if (resp_taken) d.resp_valid_i = 0;
    if (can_accept) {
      for (int i = 0; i < 6; ++i) d.resp_out_i[i] = out[i];
      d.resp_out_i[6] = 0;
      d.resp_status_i = status;
      d.resp_valid_i = 1;
    }
  }
  d.req_ready_i = 0;
  d.eval();
  return o;
}

void retire(Dut& d) {
  d.vtx_take_i = 1;
  step(d);
  d.vtx_take_i = 0;
  d.vtx_valid_i = 0;
  step(d);
  d.eval();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;
  reset(dut);

  const uint32_t kZero6[6] = {0, 0, 0, 0, 0, 0};

  // =========================================================================
  // A. THE 15-LANE CANONICAL INPUT RECORD
  // =========================================================================
  // zref_geom_warp.hpp:118-122 is the binding:
  //   0..2 px,py,pz | 3..5 nx,ny,nz | 6..9 a0..a3 | 10 time | 11..14 p0..p3
  // Checked lane by lane against NAMED values rather than against a loop that
  // regenerates the same order the RTL uses.
  {
    const uint32_t out[6] = {0x0A00'0001u, 0x0A00'0002u, 0x0A00'0003u,
                             0x0B00'0001u, 0x0B00'0002u, 0x0B00'0003u};
    Offer o = run_vertex(dut, out, 0x00);
    check(o.seen, "A: the adapter issued a request", 1, o.seen ? 1 : 0);
    check(o.lane[0] == kPx, "A lane 0: px", kPx, o.lane[0]);
    check(o.lane[1] == kPy, "A lane 1: py", kPy, o.lane[1]);
    check(o.lane[2] == kPz, "A lane 2: pz", kPz, o.lane[2]);
    check(o.lane[3] == kNx, "A lane 3: nx, the reduced NON-unit direction", kNx, o.lane[3]);
    check(o.lane[4] == kNy, "A lane 4: ny", kNy, o.lane[4]);
    check(o.lane[5] == kNz, "A lane 5: nz", kNz, o.lane[5]);
    check(o.lane[6] == kA0, "A lane 6: a0", kA0, o.lane[6]);
    check(o.lane[7] == kA1, "A lane 7: a1", kA1, o.lane[7]);
    check(o.lane[8] == kA2, "A lane 8: a2", kA2, o.lane[8]);
    check(o.lane[9] == kA3, "A lane 9: a3", kA3, o.lane[9]);
    check(o.lane[10] == kTime,
          "A lane 10: time, BIT-CAST not converted (zref_geom_warp.hpp:206-207)", kTime,
          o.lane[10]);
    check(o.lane[11] == kP0, "A lane 11: p0", kP0, o.lane[11]);
    check(o.lane[12] == kP1, "A lane 12: p1", kP1, o.lane[12]);
    check(o.lane[13] == kP2, "A lane 13: p2", kP2, o.lane[13]);
    // THE LANE THE WRONG PARENTHETICAL USED TO DROP. A build to "(14)" would
    // leave this reading whatever the host's register clear left behind.
    check(o.lane[14] == kP3,
          "A lane 14: p3 -- FIFTEEN lanes, not fourteen (decision W01)", kP3, o.lane[14]);

    // =======================================================================
    // B. THE 6 CANONICAL OUTPUT ORDINALS, PASSED THROUGH UNCHANGED
    // =======================================================================
    // W03: no add, no gain, no normalisation, no blend with the old normal.
    // The saturating add Pout = Pin + d belongs to zhao_geom_warp, not here.
    check(dut.ans_valid_o == 1, "B: an answer is available", 1, dut.ans_valid_o);
    check(dut.warp_valid_o == 1, "B: and it carries a real Field result", 1,
          dut.warp_valid_o);
    check(dut.dx_o == out[0], "B ordinal 0: dx passed through unchanged", out[0], dut.dx_o);
    check(dut.dy_o == out[1], "B ordinal 1: dy", out[1], dut.dy_o);
    check(dut.dz_o == out[2], "B ordinal 2: dz", out[2], dut.dz_o);
    check(dut.nx_o == out[3], "B ordinal 3: nx' is the REPLACEMENT normal", out[3], dut.nx_o);
    check(dut.ny_o == out[4], "B ordinal 4: ny'", out[4], dut.ny_o);
    check(dut.nz_o == out[5], "B ordinal 5: nz'", out[5], dut.nz_o);
    // The adapter must NOT have added the position into the displacement.
    check(dut.dx_o != (kPx + out[0]),
          "B: dx is NOT Pin + d -- the saturating add is zhao_geom_warp's (W03)",
          1, (dut.dx_o != (kPx + out[0])) ? 1 : 0);
    check(dut.vertices_o == 1, "B: vertices_o counted one real result", 1, dut.vertices_o);
    check(dut.identities_o == 0, "B: and it was not an identity", 0, dut.identities_o);
    retire(dut);
  }

  // =========================================================================
  // C. FH27 -- THE SIGNATURE IS CHECKED ABOVE NUMERIC EXECUTION
  // =========================================================================
  // A slot holding a FLOW program must never become a Warp request: a 13-lane
  // record read as fifteen produces six plausible numbers.
  {
    const uint32_t sig_before = dut.sig_refused_o;
    const uint32_t verts_before = dut.vertices_o;
    dut.prog_profile_i = kFlowProfile;
    dut.eval();
    Offer o = run_vertex(dut, kZero6, 0x00);
    check(!o.seen, "C FH27: NO request was issued for a non-Warp program", 0,
          o.seen ? 1 : 0);
    check(dut.sig_refused_o == sig_before + 1, "C FH27: sig_refused_o FIRED",
          sig_before + 1, dut.sig_refused_o);
    check(dut.ans_valid_o == 1, "C: the vertex was still ANSWERED, not stalled", 1,
          dut.ans_valid_o);
    check(dut.warp_valid_o == 0, "C: ... with the result ABSENT", 0, dut.warp_valid_o);
    check(dut.vertices_o == verts_before, "C: and no real result was counted",
          verts_before, dut.vertices_o);
    retire(dut);
    dut.prog_profile_i = kWarpProfile;
    dut.eval();
  }

  // =========================================================================
  // D. W10 -- IDENTITY AND REFUSAL CARRY THE SAME DATA, AND DIFFER IN VALIDITY
  // =========================================================================
  uint32_t identity_words[6];
  uint32_t bypass_words[6];
  {
    // D1. A genuine IDENTITY: the program ran, reached END, and chose d = 0.
    // n_out = n_in, which is what makes lighting exact on the disabled path.
    const uint32_t ident[6] = {0, 0, 0, kNx, kNy, kNz};
    const uint32_t id_before = dut.identities_o;
    run_vertex(dut, ident, 0x00);
    check(dut.ans_valid_o == 1, "D1 IDENTITY: answered", 1, dut.ans_valid_o);
    check(dut.warp_valid_o == 1,
          "D1 IDENTITY: warp_valid_o is HIGH -- a successful result that moves nothing", 1,
          dut.warp_valid_o);
    check(dut.identities_o == id_before + 1, "D1: identities_o fired", id_before + 1,
          dut.identities_o);
    check(dut.nx_o == kNx && dut.ny_o == kNy && dut.nz_o == kNz,
          "D1: n_out == n_in, with NO extra normalisation introduced (W03/W17)", 1,
          (dut.nx_o == kNx && dut.ny_o == kNy && dut.nz_o == kNz) ? 1 : 0);
    identity_words[0] = dut.dx_o; identity_words[1] = dut.dy_o; identity_words[2] = dut.dz_o;
    identity_words[3] = dut.nx_o; identity_words[4] = dut.ny_o; identity_words[5] = dut.nz_o;
    retire(dut);
  }
  {
    // D2. A BYPASS: no program armed at all.
    const uint32_t byp_before = dut.bypassed_o;
    dut.slot_valid_i = 0;
    dut.eval();
    Offer o = run_vertex(dut, kZero6, 0x00);
    check(!o.seen, "D2 BYPASS: no request was issued", 0, o.seen ? 1 : 0);
    check(dut.ans_valid_o == 1, "D2 BYPASS: answered immediately", 1, dut.ans_valid_o);
    check(dut.warp_valid_o == 0,
          "D2 BYPASS: warp_valid_o is LOW -- the result is ABSENT, not zero", 0,
          dut.warp_valid_o);
    check(dut.bypassed_o == byp_before + 1, "D2: bypassed_o fired, apart from every other"
          " reason", byp_before + 1, dut.bypassed_o);
    bypass_words[0] = dut.dx_o; bypass_words[1] = dut.dy_o; bypass_words[2] = dut.dz_o;
    bypass_words[3] = dut.nx_o; bypass_words[4] = dut.ny_o; bypass_words[5] = dut.nz_o;
    retire(dut);
    dut.slot_valid_i = 1;
    dut.eval();
  }
  {
    // D3. THE ASSERTION THE WHOLE FILE IS FOR.
    // The displacement words of a successful identity and of a refused run are
    // IDENTICAL, so no consumer can tell them apart from the data. Only
    // `warp_valid_o` separates them.
    const bool d_same = (identity_words[0] == bypass_words[0]) &&
                        (identity_words[1] == bypass_words[1]) &&
                        (identity_words[2] == bypass_words[2]);
    check(d_same,
          "D3 W10: an IDENTITY and a REFUSAL carry byte-identical displacement words", 1,
          d_same ? 1 : 0);
    check(identity_words[0] == 0u, "D3: ... and those words are zero", 0,
          identity_words[0]);
    // So the ONLY thing that separated them was the validity wire, which is
    // exactly what W10 requires and what a data-inspecting consumer would miss.
  }

  // D4/D5. The other two absent reasons, counted apart from each other.
  {
    const uint32_t np_before = dut.noprog_o;
    const uint32_t f_before = dut.faults_o;
    run_vertex(dut, kZero6, 0xF0);   // the host had no program in the slot
    check(dut.warp_valid_o == 0, "D4 NOPROG: result absent", 0, dut.warp_valid_o);
    check(dut.noprog_o == np_before + 1, "D4: noprog_o fired", np_before + 1, dut.noprog_o);
    check(dut.faults_o == f_before, "D4: ... and faults_o did NOT -- they are separate"
          " causes with separate fixes", f_before, dut.faults_o);
    retire(dut);
  }
  {
    const uint32_t np_before = dut.noprog_o;
    const uint32_t f_before = dut.faults_o;
    run_vertex(dut, kZero6, 0xF2);   // the run ended on an alarm
    check(dut.warp_valid_o == 0, "D5 FAULT: result absent", 0, dut.warp_valid_o);
    check(dut.faults_o == f_before + 1, "D5: faults_o fired", f_before + 1, dut.faults_o);
    check(dut.noprog_o == np_before, "D5: ... and noprog_o did not", np_before,
          dut.noprog_o);
    retire(dut);
  }
  {
    // A status the adapter has never heard of is still NOT a deformation.
    // "OK is the exact zero, not 'not one of the four I remembered'."
    const uint32_t f_before = dut.faults_o;
    run_vertex(dut, kZero6, 0x7A);
    check(dut.warp_valid_o == 0, "D6: an UNKNOWN status is not a result either", 0,
          dut.warp_valid_o);
    check(dut.faults_o == f_before + 1, "D6: counted as a fault", f_before + 1,
          dut.faults_o);
    retire(dut);
  }

  // =========================================================================
  // E. THE METADATA-SWAP GUARD, FIRED BY STIMULUS
  // =========================================================================
  // A detector reading zero is a claim. This one is made to move: the answer
  // is held, and the producer then offers a DIFFERENT vertex without taking
  // it. `held_key` was captured at request time and the live key is
  // combinational, so the two sides are clocked by different things and the
  // comparison can actually see the fault.
  {
    const uint32_t before = dut.vtx_changed_o;
    run_vertex(dut, kZero6, 0x00);
    check(dut.ans_valid_o == 1, "E: parked on a held answer", 1, dut.ans_valid_o);
    check(dut.vtx_changed_o == before,
          "E: the guard is quiet while the offered vertex is the one answered", before,
          dut.vtx_changed_o);
    // Move the vertex under the standing answer.
    dut.px_i = kPx ^ 0x00FF'0000u;
    dut.eval();
    step(dut);
    check(dut.vtx_changed_o == before + 1,
          "E: vtx_changed_o FIRED when the vertex moved under the held answer",
          before + 1, dut.vtx_changed_o);
    present_vertex(dut);
    dut.eval();
    retire(dut);
  }

  // =========================================================================
  // F. THE COST COUNTER
  // =========================================================================
  // Decision W13: three separate performance claims, none proves the other
  // two. This one is measured, and it is the gap between the host's one-point
  // front and GEOM.WARP.md's one-vertex-per-clock target.
  check(dut.stall_cycles_o > 0,
        "F: stall_cycles_o measured the scalar front's cost rather than arguing it", 1,
        (dut.stall_cycles_o > 0) ? 1 : 0);
  std::printf("  stall_cycles_o = %u over this run (evidence, not a budget)\n",
              dut.stall_cycles_o);

  return zhao::report_and_exit("field_warp_adapter_directed");
}
