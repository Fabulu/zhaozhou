// geom_pose_palette_directed.cpp — GEOM.POSE's palette store, the block that
// closes entry I10.
//
// `zhao_geom_pose_decode` emits the palette one bone per beat;
// `zhao_geom_skin` wants two whole matrices with the vertex. This block is the
// only thing in the tree that stores one and answers the other, so what it has
// to be right about is not arithmetic — it does none — but ROUTING and HOLDING:
//
//   * THE RIGHT MATRIX FOR THE RIGHT BONE. Every counter in this block balances
//     perfectly when bone0's matrix is handed over as bone1's. The differential
//     in section 1 is the only detector for that class, which is why
//     tests/mutants/zhao_geom_pose_palette_mutant.sv exists.
//   * THE WALK IS A LAW. Correct matrices delivered in fifteen cycles instead
//     of seven would pass every content check and drop GEOM.SKIN below the
//     owner-ruled 120,000 vertices/frame. Section 2 asserts the walk and
//     section 3 asserts the issue interval against GEOM.SKIN's own 12.
//   * THE RECORD HOLDS. A stalled consumer must see the same matrices beside
//     the same vertex on every cycle of the stall. Section 4 is the
//     record-swap law of CLAUDE.md, asserted as the CORRECT behaviour.
//   * A BAD BONE IS SUBSTITUTED, NOT READ. Sections 5 and 6.
//
// Every counter this block exposes is checked as a DELTA across a known
// stimulus, and every one is seen to MOVE. None is asserted zero and left.

#include "Vzhao_geom_pose_palette.h"
#include "verilated.h"

#include "geom_pose_palette_drv.hpp"
#include "zhao_sim.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

namespace {

using zhao::check;
namespace gp = geom_pose_palette;

using Dut = Vzhao_geom_pose_palette;

void section1_routing(Dut& d) {
  std::printf("\n-- 1. the right matrix for the right bone --\n");
  gp::reset(d);
  gp::pal_begin(d);
  gp::write_full_palette(d);

  // Pairs chosen so that a block which quietly used bone0 for both, or swapped
  // the two halves of the walk, cannot agree with any of them: b0 != b1 in
  // every pair, and both orders of the same pair appear.
  const int pairs[][2] = {{0, 1},  {1, 0},   {31, 0},  {0, 31}, {5, 17},
                          {17, 5}, {12, 12}, {30, 31}, {2, 29}, {29, 2}};
  int routed_a = 0, routed_b = 0, fields = 0;
  const int n = static_cast<int>(sizeof(pairs) / sizeof(pairs[0]));

  for (int t = 0; t < n; ++t) {
    gp::Offer o;
    o.b0 = static_cast<uint16_t>(pairs[t][0]);
    o.b1 = static_cast<uint16_t>(pairs[t][1]);
    o.rigid = (pairs[t][0] == pairs[t][1]);
    o.w0 = o.rigid ? 64 : static_cast<uint8_t>(1 + t * 5);
    o.x = 0x1111'0000u + static_cast<uint32_t>(t);
    o.y = 0x2222'0000u + static_cast<uint32_t>(t);
    o.z = 0x3333'0000u + static_cast<uint32_t>(t);
    o.src = static_cast<uint16_t>(0xA000 + t);

    const gp::Result r = gp::run_vertex(d, o);
    if (!r.ok) {
      check(false, "section 1: the block answered", 1, 0);
      return;
    }
    if (gp::matches_bone(r.a, pairs[t][0])) ++routed_a;
    if (gp::matches_bone(r.b, pairs[t][1])) ++routed_b;
    if (r.x == o.x && r.y == o.y && r.z == o.z && r.w0 == o.w0 && r.rigid == o.rigid &&
        r.src == o.src) {
      ++fields;
    }
  }

  check(routed_a == n, "a_m_o is bone0's matrix, every element, every pair", n, routed_a);
  check(routed_b == n, "b_m_o is bone1's matrix, every element, every pair", n, routed_b);
  check(fields == n, "the vertex rides through unchanged beside its matrices", n, fields);
}

void section2_walk(Dut& d) {
  std::printf("\n-- 2. the walk is exactly seven clocks --\n");
  gp::reset(d);
  gp::pal_begin(d);
  gp::write_full_palette(d);

  int same = 0;
  int observed = -1;
  for (int t = 0; t < 6; ++t) {
    gp::Offer o;
    o.b0 = static_cast<uint16_t>(t);
    o.b1 = static_cast<uint16_t>(31 - t);
    o.rigid = false;
    o.w0 = 32;
    const gp::Result r = gp::run_vertex(d, o);
    if (t == 0) observed = r.accept_to_valid;
    if (r.accept_to_valid == gp::kWalk) ++same;
  }
  check(same == 6, "accept to o_valid_o is the derived walk on every vertex", gp::kWalk,
        static_cast<uint64_t>(observed));

  // The write walk is its own law: three rows, one row per clock, and the beat
  // completes on the third. A four-clock beat would still be correct and would
  // still be worth knowing about.
  const int beat = gp::write_bone(d, 7, 7);
  check(beat == 3, "one palette beat is three clocks", 3, static_cast<uint64_t>(beat));
}

void section3_issue_interval(Dut& d) {
  std::printf("\n-- 3. the issue interval stays under GEOM.SKIN's twelve --\n");
  gp::reset(d);
  gp::pal_begin(d);
  gp::write_full_palette(d);

  // A never-stalling consumer and a never-empty producer: hold both handshakes
  // asserted and count the clocks between accepts.
  d.o_ready_i = 1;
  d.v_valid_i = 1;
  d.v_rigid_i = 0;
  d.v_w0_i = 20;
  d.v_bone0_i = 3;
  d.v_bone1_i = 9;
  d.v_x_i = 1;
  d.v_y_i = 2;
  d.v_z_i = 3;
  d.v_src_id_i = 0x55;
  d.eval();

  const int kVerts = 20;
  int accepts = 0;
  int clocks = 0;
  int first_accept_clock = -1;
  int last_accept_clock = -1;
  while (clocks < 4000 && accepts < kVerts) {
    if (d.v_valid_i && d.v_ready_o) {
      if (first_accept_clock < 0) first_accept_clock = clocks;
      last_accept_clock = clocks;
      ++accepts;
    }
    zhao::tick(d);
    ++clocks;
  }
  d.v_valid_i = 0;
  d.o_ready_i = 0;
  d.eval();

  check(accepts == kVerts, "the stream runs: twenty vertices accepted", kVerts,
        static_cast<uint64_t>(accepts));

  int ii = -1;
  if (accepts == kVerts && first_accept_clock >= 0) {
    ii = (last_accept_clock - first_accept_clock) / (kVerts - 1);
  }
  std::printf("   measured issue interval: %d clocks/vertex (GEOM.SKIN's is %d)\n", ii,
              gp::kSkinII);
  check(ii > 0 && ii < gp::kSkinII,
        "issue interval is strictly under GEOM.SKIN's, so this block is not the bottleneck", 1,
        (ii > 0 && ii < gp::kSkinII) ? 1 : 0);

  // The rate this block sustains, against the owner ruling. 100 MHz,
  // 1,666,666 clocks per 60 Hz frame.
  if (ii > 0) {
    const long vps = 1666666L / ii;
    std::printf("   => %ld vertices/frame at 100 MHz (owner ruling: 120,000)\n", vps);
    check(vps >= 120000, "this block alone sustains the owner-ruled vertex budget", 120000,
          static_cast<uint64_t>(vps));
  }
}

void section4_hold(Dut& d) {
  std::printf("\n-- 4. the record holds through a stall --\n");
  gp::reset(d);
  gp::pal_begin(d);
  gp::write_full_palette(d);

  // Offer a vertex and walk it out, but DO NOT consume it. Then push a second,
  // different vertex at the input for the whole stall.
  d.o_ready_i = 0;
  d.v_valid_i = 1;
  d.v_x_i = 0xDEAD'0001u;
  d.v_y_i = 0xDEAD'0002u;
  d.v_z_i = 0xDEAD'0003u;
  d.v_w0_i = 11;
  d.v_rigid_i = 0;
  d.v_bone0_i = 4;
  d.v_bone1_i = 21;
  d.v_src_id_i = 0x0BED;
  d.eval();
  int guard = 0;
  while (!d.v_ready_o && guard++ < 64) zhao::tick(d);
  zhao::tick(d);  // accept
  d.v_valid_i = 0;
  d.eval();
  guard = 0;
  while (!d.o_valid_o && guard++ < 64) zhao::tick(d);
  check(d.o_valid_o != 0, "section 4: the offer arrived", 1, d.o_valid_o ? 1 : 0);

  const uint32_t served_before = d.vertices_served_o;

  // Present a DIFFERENT vertex naming DIFFERENT bones for the whole stall. A
  // block that accepted it into the same registers would swap the record.
  d.v_valid_i = 1;
  d.v_x_i = 0xFEED'0001u;
  d.v_y_i = 0xFEED'0002u;
  d.v_z_i = 0xFEED'0003u;
  d.v_w0_i = 60;
  d.v_rigid_i = 1;
  d.v_bone0_i = 30;
  d.v_bone1_i = 30;
  d.v_src_id_i = 0xF00D;
  d.eval();

  int held = 0;
  int ready_low = 0;
  const int kStall = 25;
  for (int c = 0; c < kStall; ++c) {
    bool same = (d.o_valid_o != 0) && gp::port_matches_bone(d.a_m_o, 4) &&
                gp::port_matches_bone(d.b_m_o, 21) && d.o_x_o == 0xDEAD'0001u &&
                d.o_y_o == 0xDEAD'0002u && d.o_z_o == 0xDEAD'0003u && d.o_w0_o == 11 &&
                d.o_rigid_o == 0 && d.o_src_id_o == 0x0BED;
    if (same) ++held;
    if (!d.v_ready_o) ++ready_low;
    zhao::tick(d);
  }
  check(held == kStall, "the held offer is byte-identical on every stalled cycle", kStall,
        static_cast<uint64_t>(held));
  check(ready_low == kStall, "v_ready_o is low for the whole stall: no second vertex is taken",
        kStall, static_cast<uint64_t>(ready_low));
  check(d.vertices_served_o == served_before,
        "vertices_served_o does not advance on an unconsumed offer", served_before,
        d.vertices_served_o);

  // Release. The SECOND vertex must now be the one that flows, with its own
  // bones — the stall must not have silently discarded or duplicated it.
  d.o_ready_i = 1;
  d.eval();
  zhao::tick(d);
  check(d.vertices_served_o == served_before + 1,
        "vertices_served_o advances exactly once on the handoff", served_before + 1,
        d.vertices_served_o);
  d.o_ready_i = 0;
  d.eval();
  guard = 0;
  while (!d.o_valid_o && guard++ < 64) zhao::tick(d);
  const bool second_ok = (d.o_valid_o != 0) && gp::port_matches_bone(d.a_m_o, 30) &&
                         gp::port_matches_bone(d.b_m_o, 30) && d.o_src_id_o == 0xF00D;
  check(second_ok, "the vertex that was waiting is served next, with ITS bones", 1,
        second_ok ? 1 : 0);
  d.v_valid_i = 0;
  d.o_ready_i = 1;
  d.eval();
  zhao::tick(d);
  d.o_ready_i = 0;
  d.eval();
}

void section5_counters(Dut& d) {
  std::printf("\n-- 5. every counter, as a delta --\n");
  gp::reset(d);
  gp::pal_begin(d);

  // bones_written_o
  const uint32_t bw0 = d.bones_written_o;
  for (int b = 0; b < gp::kBones; ++b) gp::write_bone(d, b, b);
  check(d.bones_written_o == bw0 + gp::kBones, "bones_written_o advances once per beat",
        bw0 + gp::kBones, d.bones_written_o);

  // vertices_served_o
  const uint32_t vs0 = d.vertices_served_o;
  for (int t = 0; t < 4; ++t) {
    gp::Offer o;
    o.b0 = static_cast<uint16_t>(t);
    o.b1 = static_cast<uint16_t>(t + 1);
    o.rigid = false;
    gp::run_vertex(d, o);
  }
  check(d.vertices_served_o == vs0 + 4, "vertices_served_o advances once per vertex", vs0 + 4,
        d.vertices_served_o);

  // bone_oob_o — one bad index, then two. It counts REFERENCES, so a vertex
  // naming two bad bones moves it by two.
  uint32_t ob = d.bone_oob_o;
  gp::Offer one_bad;
  one_bad.b0 = 40;  // past a 32-bone palette
  one_bad.b1 = 6;
  one_bad.rigid = false;
  gp::Result r = gp::run_vertex(d, one_bad);
  check(d.bone_oob_o == ob + 1, "bone_oob_o moves by one for one out-of-range bone", ob + 1,
        d.bone_oob_o);
  check(gp::matches_identity(r.a), "the out-of-range bone is served the identity bind pose", 1,
        gp::matches_identity(r.a) ? 1 : 0);
  check(gp::matches_bone(r.b, 6), "its legal sibling is still served its real matrix", 1,
        gp::matches_bone(r.b, 6) ? 1 : 0);

  ob = d.bone_oob_o;
  gp::Offer two_bad;
  two_bad.b0 = 32;  // the first index past the end
  two_bad.b1 = 0xFFFF;
  two_bad.rigid = false;
  r = gp::run_vertex(d, two_bad);
  check(d.bone_oob_o == ob + 2, "bone_oob_o moves by two when both bones are out of range", ob + 2,
        d.bone_oob_o);
  check(gp::matches_identity(r.a) && gp::matches_identity(r.b),
        "both halves are the identity bind pose", 1,
        (gp::matches_identity(r.a) && gp::matches_identity(r.b)) ? 1 : 0);

  // The vertex still FLOWS. A dropped vertex would punch a hole in a stream
  // whose consumers count beats.
  const uint32_t vs1 = d.vertices_served_o;
  gp::run_vertex(d, two_bad);
  check(d.vertices_served_o == vs1 + 1, "a vertex with two bad bones is still served, not dropped",
        vs1 + 1, d.vertices_served_o);
}

void section6_residency(Dut& d) {
  std::printf("\n-- 6. residency: pal_begin makes the old pose unreadable --\n");
  gp::reset(d);

  // Cold, after reset and before any write: NOTHING is resident, so every read
  // is the bind pose and bone_unset_o fires. This is also the misuse detector —
  // a vertex arriving during a decode lands here.
  uint32_t ub = d.bone_unset_o;
  gp::Offer cold;
  cold.b0 = 3;
  cold.b1 = 9;
  cold.rigid = false;
  gp::Result r = gp::run_vertex(d, cold);
  check(d.bone_unset_o == ub + 2, "bone_unset_o moves by two on a cold palette", ub + 2,
        d.bone_unset_o);
  check(gp::matches_identity(r.a) && gp::matches_identity(r.b),
        "a non-resident bone is served the identity bind pose", 1,
        (gp::matches_identity(r.a) && gp::matches_identity(r.b)) ? 1 : 0);

  // Write the palette; now it is silent and the real matrices come back.
  gp::write_full_palette(d);
  ub = d.bone_unset_o;
  r = gp::run_vertex(d, cold);
  check(d.bone_unset_o == ub, "bone_unset_o is silent once the bones are resident", ub,
        d.bone_unset_o);
  check(gp::matches_bone(r.a, 3) && gp::matches_bone(r.b, 9),
        "the resident palette reads back exactly", 1,
        (gp::matches_bone(r.a, 3) && gp::matches_bone(r.b, 9)) ? 1 : 0);

  // pal_begin clears residency for EVERY bone in one cycle.
  gp::pal_begin(d);
  ub = d.bone_unset_o;
  r = gp::run_vertex(d, cold);
  check(d.bone_unset_o == ub + 2, "pal_begin_i makes the previous pose unreadable", ub + 2,
        d.bone_unset_o);
  check(gp::matches_identity(r.a), "and the bind pose is what is served instead", 1,
        gp::matches_identity(r.a) ? 1 : 0);

  // A bone re-written after the begin is readable again; its neighbour is not.
  // This is the partial-decode state a real frame passes through.
  gp::write_bone(d, 3, 3);
  ub = d.bone_unset_o;
  r = gp::run_vertex(d, cold);
  check(d.bone_unset_o == ub + 1, "only the bone that is still missing is counted", ub + 1,
        d.bone_unset_o);
  check(gp::matches_bone(r.a, 3) && gp::matches_identity(r.b),
        "the re-written bone reads, the missing one takes the bind pose", 1,
        (gp::matches_bone(r.a, 3) && gp::matches_identity(r.b)) ? 1 : 0);

  // A bone is NOT resident until its last row lands. Two rows in is still a
  // read of the bind pose, which is what makes the read-during-write collision
  // harmless (see the RTL header).
  gp::pal_begin(d);
  for (int i = 0; i < gp::kElems; ++i) d.wr_m_i[i] = gp::pal_elem(5, i);
  d.wr_bone_i = 5;
  d.wr_valid_i = 1;
  d.eval();
  zhao::tick(d);  // row 0
  zhao::tick(d);  // row 1 — two of three rows written, beat not complete
  d.wr_valid_i = 0;
  d.eval();
  ub = d.bone_unset_o;
  gp::Offer mid;
  mid.b0 = 5;
  mid.b1 = 5;
  mid.rigid = true;
  r = gp::run_vertex(d, mid);
  check(d.bone_unset_o == ub + 2, "a half-written bone is not resident", ub + 2, d.bone_unset_o);
  check(gp::matches_identity(r.a), "and a read of it never sees a torn matrix", 1,
        gp::matches_identity(r.a) ? 1 : 0);
}

void section7_ready_is_not_a_function_of_valid(Dut& d) {
  std::printf("\n-- 7. ready never depends on valid --\n");
  gp::reset(d);
  gp::pal_begin(d);
  gp::write_full_palette(d);

  // A ready that is computed from valid is a combinational loop waiting for a
  // producer that does the same. Both handshakes are checked the only way a
  // black box can: move valid, settle, and look at ready.
  int stable_v = 0, stable_w = 0;
  const int kProbes = 8;
  for (int c = 0; c < kProbes; ++c) {
    d.v_valid_i = 0;
    d.wr_valid_i = 0;
    d.eval();
    const int vr0 = d.v_ready_o;
    const int wr0 = d.wr_ready_o;
    d.v_valid_i = 1;
    d.wr_valid_i = 1;
    d.v_bone0_i = static_cast<uint16_t>(c);
    d.v_bone1_i = static_cast<uint16_t>(c + 1);
    d.wr_bone_i = static_cast<uint8_t>(c);
    d.eval();
    if (d.v_ready_o == vr0) ++stable_v;
    if (d.wr_ready_o == wr0) ++stable_w;
    zhao::tick(d);
  }
  d.v_valid_i = 0;
  d.wr_valid_i = 0;
  d.eval();
  check(stable_v == kProbes, "v_ready_o does not move when v_valid_i does", kProbes,
        static_cast<uint64_t>(stable_v));
  check(stable_w == kProbes, "wr_ready_o does not move when wr_valid_i does", kProbes,
        static_cast<uint64_t>(stable_w));
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;

  std::printf("geom_pose_palette_directed — GEOM.POSE's palette store (entry I10)\n");

  section1_routing(dut);
  section2_walk(dut);
  section3_issue_interval(dut);
  section4_hold(dut);
  section5_counters(dut);
  section6_residency(dut);
  section7_ready_is_not_a_function_of_valid(dut);

  const int rc = zhao::report_and_exit("geom_pose_palette_directed");
  zhao::exit_hard(rc);
}
