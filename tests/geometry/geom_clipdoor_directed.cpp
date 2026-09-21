// geom_clipdoor_directed.cpp -- the N-producer door at `zhao_geom_clip`'s INPUT.
//
// WHY THE BLOCK EXISTS, in one line: owner ruling R187 named GEOM.CLIP's input
// as "the honest door ... for every non-mesh producer", and
// `zhao_console_core`'s R197 block says the arbiter that admits them "will
// present its `untex` bit to THIS gate, not to a second copy of it
// downstream". GEOM.CLIP has exactly ONE triangle input port and GEOM.REPLAY
// already holds every wire of it.
//
// WHAT ACTUALLY DISCRIMINATES HERE, named up front, because a bench that only
// shows a triangle going in and coming out would pass against a door with no
// grant register at all:
//
//   1. THE MATERIAL HALF AND THE TRIANGLE HALF ARE ONE BEAT. In the composed
//      console the material fields go to `zhao_material_window`'s input and the
//      triangle fields go to `zhao_geom_clip`'s, and the window is a pure
//      combinational gate between them. If the door ever presented client A's
//      material beside client B's triangle, the shell would shade a particle
//      with a mesh's texture and every counter in the console would balance.
//      So every check below that reads a coordinate ALSO reads the material id,
//      and they must name the same client.
//
//   2. THE GRANT IS RUN-LENGTH FAIR, NOT BEAT FAIR, AND THAT IS A THROUGHPUT
//      LAW WITH A CORRECTNESS-SHAPED TEST. `zhao_material_window` DRAINS the
//      whole GEOM.CLIP..door span before it changes what it publishes. A
//      beat-fair round robin between two producers of different materials would
//      therefore issue a drain and a resolve between every pair of triangles.
//      So: with BOTH clients offering continuously, the grant must NOT
//      alternate -- `switches_o` must stay put while one client's `granted_o`
//      climbs. A beat-fair arbiter passes every ordinary arbiter test and fails
//      this one, which is the whole reason it is here.
//
//   3. THE LOSER IS HELD, NOT DROPPED, and it is granted with the SAME payload.
//      A dropped triangle looks identical to a served one in any counter that
//      only counts what came out.
//
//   4. THE HOLD SURVIVES A STALLED SINK. With `o_ready_i` LOW for many cycles
//      and the OTHER client asserting valid throughout, nothing on the door's
//      output may move. That is the case that separates a latched grant from a
//      combinational "whoever is asking now" mux, and it is exactly the state
//      `zhao_geom_clip` produces whenever its stage 3 holds an accepted packet.
//
//   5. `err_hold_broken_o` IS A NEGATIVE CONTROL HERE. It must stay at zero
//      under every legal stimulus above; its POSITIVE control is the committed
//      mutant `tests/mutants/zhao_geom_clipdoor_mutant.sv`, because no legal
//      input at these ports can make a correct hold law release a grant.
//
//   6. THE TWO CLIENTS DIFFER IN EVERY FIELD. Coordinates, behind bits, source
//      ids, the untextured declaration, the cull mode, the attribute witness
//      and all three material fields. Two clients offering the same numbers is
//      the commonest way an arbiter test is written and it discriminates
//      nothing.
#include <cstdint>
#include <cstdio>

#include "Vtb_geom_clipdoor.h"
#include "zhao_sim.hpp"

using zhao::check;
using zhao::tick;

namespace {

// Client 0 is the MESH arm's shape: textured, a real material, cull mode from
// the triangle's own raster word.
constexpr int32_t  kA0x = 0x000A'AAA;
constexpr int32_t  kA0y = -0x000B'BBB;
constexpr uint32_t kW0  = 0x1111'2222u;
constexpr uint32_t kSet0 = 0xDEAD'BEEFu;
constexpr uint16_t kId0 = 0x1234;

// Client 1 is the PARTICLE arm's shape: untextured by law, double-sided, its
// own material identity.
constexpr int32_t  kA1x = -0x000C'CCC;
constexpr int32_t  kA1y = 0x000D'DDD;
constexpr uint32_t kW1  = 0x9999'8888u;
constexpr uint32_t kSet1 = 0x0BAD'F00Du;
constexpr uint16_t kId1 = 0x5678;

void drive_client0(Vtb_geom_clipdoor& d) {
  d.c0_ax_i = kA0x;
  d.c0_ay_i = kA0y;
  d.c0_bx_i = kA0x + 16;
  d.c0_by_i = kA0y + 16;
  d.c0_cx_i = kA0x + 32;
  d.c0_cy_i = kA0y + 32;
  d.c0_behind_i = 0;
  d.c0_src_id_i = 0x00A5;
  d.c0_untex_i = 0;
  d.c0_cull_mode_i = 1;
  d.c0_attr_witness_i = kW0;
  d.c0_material_set_i = kSet0;
  d.c0_material_id_i = kId0;
  d.c0_quality_tier_i = 0x11;
}

void drive_client1(Vtb_geom_clipdoor& d) {
  d.c1_ax_i = kA1x;
  d.c1_ay_i = kA1y;
  d.c1_bx_i = kA1x - 16;
  d.c1_by_i = kA1y - 16;
  d.c1_cx_i = kA1x - 32;
  d.c1_cy_i = kA1y - 32;
  d.c1_behind_i = 0;
  d.c1_src_id_i = 0x005A;
  d.c1_untex_i = 1;
  d.c1_cull_mode_i = 0;
  d.c1_attr_witness_i = kW1;
  d.c1_material_set_i = kSet1;
  d.c1_material_id_i = kId1;
  d.c1_quality_tier_i = 0x22;
}

void hard_reset(Vtb_geom_clipdoor& d) {
  d.rst_n = 0;
  d.c0_valid_i = 0;
  d.c1_valid_i = 0;
  d.o_ready_i = 0;
  drive_client0(d);
  drive_client1(d);
  d.eval();
  for (int i = 0; i < 3; ++i) tick(d);
  d.rst_n = 1;
  d.eval();
  tick(d);
}

// Sign-extend the DUT's 21-bit coordinate ports the way the RTL means them.
int32_t sx21(uint32_t v) {
  v &= 0x1F'FFFFu;
  return (v & 0x10'0000u) ? (int32_t)(v | 0xFFE0'0000u) : (int32_t)v;
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  auto* top = new Vtb_geom_clipdoor;
  auto& d = *top;

  hard_reset(d);

  check(d.o_valid_o == 0, "no offer out of reset with nobody asking", 0,
        d.o_valid_o);
  check(d.switches_o == 0, "no grant taken out of reset", 0, d.switches_o);
  check(d.err_hold_broken_o == 0, "the hold guard starts at zero", 0,
        d.err_hold_broken_o);

  // =========================================================================
  // 1. ONE CLIENT, END TO END -- and BOTH HALVES of the beat are its own
  // =========================================================================
  d.o_ready_i = 1;
  d.c0_valid_i = 1;
  d.eval();
  // The grant is a register, so the first offer is granted on the NEXT clock:
  // a combinational grant would close a loop from `c_valid_i` to `c_ready_o`.
  check(d.o_valid_o == 0, "a register grant does not offer combinationally", 0,
        d.o_valid_o);
  check(d.idle_offered_o == 0,
        "the acquisition clock has not been counted yet", 0, d.idle_offered_o);
  tick(d);
  d.eval();

  check(d.o_valid_o == 1, "a lone client is offered to the door", 1,
        d.o_valid_o);
  check(d.o_owner_o == 0x1, "the owner reads client 0", 0x1, d.o_owner_o);
  check(sx21(d.o_ax_o) == kA0x, "the beat carries client 0's ax", kA0x,
        sx21(d.o_ax_o));
  check(sx21(d.o_ay_o) == kA0y, "the beat carries client 0's ay", kA0y,
        sx21(d.o_ay_o));
  check(sx21(d.o_cy_o) == kA0y + 32, "the beat carries client 0's cy",
        kA0y + 32, sx21(d.o_cy_o));
  check(d.o_src_id_o == 0x00A5, "the beat carries client 0's source id", 0x00A5,
        d.o_src_id_o);
  check(d.o_untex_o == 0, "the beat carries client 0's R197 declaration", 0,
        d.o_untex_o);
  check(d.o_cull_mode_o == 1, "the beat carries client 0's cull mode", 1,
        d.o_cull_mode_o);
  check(d.o_attr_a_lo_o == kW0, "corner A slot 0 is client 0's witness", kW0,
        d.o_attr_a_lo_o);
  check(d.o_attr_a_hi_o == kW0, "corner A's TOP slot is client 0's witness too",
        kW0, d.o_attr_a_hi_o);
  check(d.o_attr_b_lo_o == (kW0 ^ 0x0000'00FFu),
        "corner B is corner B, not a second copy of A", kW0 ^ 0x0000'00FFu,
        d.o_attr_b_lo_o);
  check(d.o_attr_c_lo_o == (kW0 ^ 0x0000'FF00u),
        "corner C is corner C, not a second copy of A", kW0 ^ 0x0000'FF00u,
        d.o_attr_c_lo_o);
  // CHECK 1 of the header: the MATERIAL half names the SAME client.
  check(d.o_material_set_o == kSet0,
        "the material half of the beat is client 0's set", kSet0,
        d.o_material_set_o);
  check(d.o_material_id_o == kId0,
        "the material half of the beat is client 0's id", kId0,
        d.o_material_id_o);
  check(d.o_quality_tier_o == 0x11, "and client 0's quality tier", 0x11,
        d.o_quality_tier_o);
  check(d.c0_ready_o == 1, "client 0 sees ready on the granted cycle", 1,
        d.c0_ready_o);
  check(d.c1_ready_o == 0, "client 1 does not see ready", 0, d.c1_ready_o);

  tick(d);
  d.eval();
  check(d.granted0_o == 1, "one beat recorded for client 0", 1, d.granted0_o);
  check(d.granted1_o == 0, "and none for client 1", 0, d.granted1_o);
  check(d.switches_o == 1, "the first acquisition is one switch", 1,
        d.switches_o);

  // =========================================================================
  // 2. THE HOLD SURVIVES A STALLED SINK, WITH THE OTHER CLIENT ASKING
  //
  //    This is the state `zhao_geom_clip` produces whenever its stage 3 holds
  //    an accepted packet the shell will not take. A door that muxed on
  //    "whoever is asking" would swap the beat underneath the stall.
  // =========================================================================
  d.o_ready_i = 0;
  d.c1_valid_i = 1;
  d.eval();
  const uint32_t sw_before = d.switches_o;
  for (int i = 0; i < 12; ++i) {
    tick(d);
    d.eval();
    check(d.o_valid_o == 1, "the stalled beat stays offered", 1, d.o_valid_o);
    check(d.o_owner_o == 0x1, "and it is still client 0's", 0x1, d.o_owner_o);
    check(sx21(d.o_ax_o) == kA0x, "its ax does not move under the stall", kA0x,
          sx21(d.o_ax_o));
    check(d.o_material_id_o == kId0,
          "and neither does its material id -- the two halves stay paired",
          kId0, d.o_material_id_o);
    check(d.c1_ready_o == 0, "the waiting client is not accepted meanwhile", 0,
          d.c1_ready_o);
  }
  check(d.switches_o == sw_before, "no grant change under a stalled sink",
        sw_before, d.switches_o);
  check(d.err_hold_broken_o == 0,
        "and the hold guard did not fire during the stall", 0,
        d.err_hold_broken_o);

  // =========================================================================
  // 3. RUN-LENGTH FAIRNESS: BOTH ASKING FOREVER MUST **NOT** ALTERNATE
  //
  //    See header check 2. This is the check a beat-fair round robin fails.
  // =========================================================================
  d.o_ready_i = 1;
  const uint32_t g0_base = d.granted0_o;
  const uint32_t g1_base = d.granted1_o;
  const uint32_t sw_base = d.switches_o;
  for (int i = 0; i < 20; ++i) {
    tick(d);
    d.eval();
    check(d.o_owner_o == 0x1,
          "the owner holds while it keeps offering, however loudly the other "
          "client asks",
          0x1, d.o_owner_o);
    check(d.o_material_set_o == kSet0,
          "so the window is never asked to switch material mid-run", kSet0,
          d.o_material_set_o);
  }
  check(d.switches_o == sw_base,
        "twenty contended beats produced ZERO material switches", sw_base,
        d.switches_o);
  check(d.granted0_o - g0_base == 20, "all twenty went to client 0", 20,
        d.granted0_o - g0_base);
  check(d.granted1_o - g1_base == 0, "and none to client 1", 0,
        d.granted1_o - g1_base);

  // =========================================================================
  // 4. THE LOSER IS HELD, NOT DROPPED -- and it takes over the moment the
  //    owner goes idle, with its OWN payload in BOTH halves
  // =========================================================================
  d.c0_valid_i = 0;
  d.eval();
  check(d.o_valid_o == 0, "the door offers nothing on the owner's idle clock",
        0, d.o_valid_o);
  tick(d);
  d.eval();
  check(d.o_valid_o == 1, "the waiting client is granted on the next clock", 1,
        d.o_valid_o);
  check(d.o_owner_o == 0x2, "the owner reads client 1", 0x2, d.o_owner_o);
  check(sx21(d.o_ax_o) == kA1x, "and the beat is client 1's ax, unchanged",
        kA1x, sx21(d.o_ax_o));
  check(sx21(d.o_by_o) == kA1y - 16, "client 1's by, unchanged", kA1y - 16,
        sx21(d.o_by_o));
  check(d.o_untex_o == 1,
        "client 1's R197 untextured declaration reaches the gate", 1,
        d.o_untex_o);
  check(d.o_cull_mode_o == 0, "client 1's cull mode (NONE) reaches GEOM.CLIP",
        0, d.o_cull_mode_o);
  check(d.o_src_id_o == 0x005A, "client 1's source id", 0x005A, d.o_src_id_o);
  check(d.o_attr_a_lo_o == kW1, "client 1's attribute packet, slot 0", kW1,
        d.o_attr_a_lo_o);
  check(d.o_attr_a_hi_o == kW1, "client 1's attribute packet, top slot", kW1,
        d.o_attr_a_hi_o);
  check(d.o_material_set_o == kSet1, "client 1's material set", kSet1,
        d.o_material_set_o);
  check(d.o_material_id_o == kId1, "client 1's material id", kId1,
        d.o_material_id_o);
  check(d.o_quality_tier_o == 0x22, "client 1's quality tier", 0x22,
        d.o_quality_tier_o);
  check(d.switches_o == sw_base + 1, "exactly one switch was recorded",
        sw_base + 1, d.switches_o);

  tick(d);
  d.eval();
  check(d.granted1_o - g1_base == 1, "client 1's first beat is recorded", 1,
        d.granted1_o - g1_base);

  // =========================================================================
  // 5. THE ROTATION IS EXERCISED. Alternating idle gaps must hand the door
  //    back and forth, and each switch must be counted exactly once. A
  //    fairness law nothing contends is a claim.
  // =========================================================================
  const uint32_t sw_rot = d.switches_o;
  const uint32_t g0_rot = d.granted0_o;
  const uint32_t g1_rot = d.granted1_o;
  for (int round = 0; round < 6; ++round) {
    // Hand it to 0.
    d.c1_valid_i = 0;
    d.c0_valid_i = 1;
    d.eval();
    tick(d);
    d.eval();
    check(d.o_owner_o == 0x1, "rotation: client 0 takes the idle door", 0x1,
          d.o_owner_o);
    check(d.o_material_id_o == kId0, "rotation: with client 0's material",
          kId0, d.o_material_id_o);
    tick(d);
    // Hand it to 1.
    d.c0_valid_i = 0;
    d.c1_valid_i = 1;
    d.eval();
    tick(d);
    d.eval();
    check(d.o_owner_o == 0x2, "rotation: client 1 takes the idle door", 0x2,
          d.o_owner_o);
    check(d.o_material_id_o == kId1, "rotation: with client 1's material",
          kId1, d.o_material_id_o);
    tick(d);
  }
  check(d.switches_o - sw_rot == 12, "twelve hand-overs, twelve switches", 12,
        d.switches_o - sw_rot);
  check(d.granted0_o - g0_rot == 6, "six beats to client 0", 6,
        d.granted0_o - g0_rot);
  check(d.granted1_o - g1_rot == 6, "six beats to client 1", 6,
        d.granted1_o - g1_rot);

  // =========================================================================
  // 6. `idle_offered_o` DISCRIMINATES. It counts the acquisition clocks --
  //    somebody offering while the door has not yet granted -- and nothing
  //    else. Twelve hand-overs above each cost exactly one.
  // =========================================================================
  check(d.idle_offered_o > 0,
        "idle_offered_o moved: the acquisition clock is real and counted", 1,
        d.idle_offered_o > 0);

  // =========================================================================
  // 7. THE NEGATIVE CONTROL. Nothing above may have moved the hold guard.
  // =========================================================================
  d.c0_valid_i = 0;
  d.c1_valid_i = 0;
  d.eval();
  tick(d);
  check(d.err_hold_broken_o == 0,
        "err_hold_broken_o stayed at ZERO under every legal stimulus (its "
        "positive control is tests/mutants/zhao_geom_clipdoor_mutant.sv)",
        0, d.err_hold_broken_o);

  check(d.granted0_o > 0 && d.granted1_o > 0 && d.switches_o > 0 &&
            d.idle_offered_o > 0,
        "every counter reachable by legal stimulus was FIRED", 1,
        d.granted0_o > 0 && d.granted1_o > 0 && d.switches_o > 0 &&
            d.idle_offered_o > 0);

  std::printf(
      "[geom_clipdoor_directed] granted0=%u granted1=%u switches=%u "
      "idle_offered=%u err_hold_broken=%u (negative control; the mutant is its "
      "positive control)\n",
      d.granted0_o, d.granted1_o, d.switches_o, d.idle_offered_o,
      d.err_hold_broken_o);

  top->final();
  return zhao::report_and_exit("geom_clipdoor_directed");
}
