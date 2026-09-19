// geom_pose_palette_mutant_control.cpp — the INVERTED-POLARITY control for
// tests/mutants/zhao_geom_pose_palette_mutant.sv.
//
// It PASSES when the mutant is caught. It is evidence about the directed
// suite's section-1 differential, not about the shipped block.
//
// The claim being demonstrated is the one CLAUDE.md warns is easiest to get
// away with: **every counter this block owns balances while the routing is
// wrong.** So the control asserts both halves — that the differential goes red
// AND that not one counter notices — because half of that is the whole reason
// the differential has to exist.

#include "Vzhao_geom_pose_palette_mutant.h"
#include "verilated.h"

#include "geom_pose_palette_drv.hpp"
#include "zhao_sim.hpp"

#include <cstdint>
#include <cstdio>

namespace {

using zhao::check;
namespace gp = geom_pose_palette;

using Dut = Vzhao_geom_pose_palette_mutant;

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;

  std::printf("geom_pose_palette_mutant_control — INVERTED POLARITY\n");
  std::printf("  the routing differential must FAIL here; the counters must NOT\n");

  gp::reset(dut);
  gp::pal_begin(dut);
  gp::write_full_palette(dut);

  // Exactly section 1's pairs, from the shared driver. Only the pairs where
  // bone0 != bone1 can reveal this mutation; the rigid pair cannot, and that
  // is itself worth saying out loud.
  const int pairs[][2] = {{0, 1}, {1, 0}, {31, 0}, {0, 31}, {5, 17}, {17, 5}, {2, 29}};
  const int n = static_cast<int>(sizeof(pairs) / sizeof(pairs[0]));

  const uint32_t served0 = dut.vertices_served_o;
  const uint32_t oob0 = dut.bone_oob_o;
  const uint32_t unset0 = dut.bone_unset_o;
  const uint32_t written0 = dut.bones_written_o;

  int a_right = 0;
  int b_wrong = 0;
  int walk_right = 0;

  for (int t = 0; t < n; ++t) {
    gp::Offer o;
    o.b0 = static_cast<uint16_t>(pairs[t][0]);
    o.b1 = static_cast<uint16_t>(pairs[t][1]);
    o.rigid = false;
    o.w0 = 20;
    o.src = static_cast<uint16_t>(0xC000 + t);
    const gp::Result r = gp::run_vertex(dut, o);
    if (!r.ok) {
      check(false, "the mutant answered at all", 1, 0);
      break;
    }
    if (gp::matches_bone(r.a, pairs[t][0])) ++a_right;
    if (!gp::matches_bone(r.b, pairs[t][1])) ++b_wrong;
    if (r.accept_to_valid == gp::kWalk) ++walk_right;
  }

  // THE DETECTION. b_m_o is wrong on every pair with two distinct bones.
  check(b_wrong == n, "INVERTED: b_m_o is NOT bone1's matrix on every distinct pair", n,
        static_cast<uint64_t>(b_wrong));

  // …and it is wrong in the specific way the mutation makes it wrong, which is
  // what separates "the differential went red" from "the differential went red
  // for the reason claimed".
  gp::Offer probe;
  probe.b0 = 7;
  probe.b1 = 23;
  probe.rigid = false;
  const gp::Result pr = gp::run_vertex(dut, probe);
  check(gp::matches_bone(pr.b, 7),
        "INVERTED: b_m_o carries BONE0's matrix, which is the mutation's signature", 1,
        gp::matches_bone(pr.b, 7) ? 1 : 0);

  // THE POINT. Nothing else moved. Everything that is not the differential
  // reports a healthy block.
  check(a_right == n, "a_m_o is still correct, so half the routing looks fine", n,
        static_cast<uint64_t>(a_right));
  check(walk_right == n, "the walk is still exactly seven clocks", n,
        static_cast<uint64_t>(walk_right));
  check(dut.vertices_served_o == served0 + static_cast<uint32_t>(n) + 1,
        "vertices_served_o still advances once per vertex", served0 + n + 1, dut.vertices_served_o);
  check(dut.bone_oob_o == oob0, "bone_oob_o is silent, as it should be on legal indices", oob0,
        dut.bone_oob_o);
  check(dut.bone_unset_o == unset0, "bone_unset_o is silent on a fully resident palette", unset0,
        dut.bone_unset_o);
  check(dut.bones_written_o == written0, "bones_written_o did not move during the reads", written0,
        dut.bones_written_o);

  const int rc = zhao::report_and_exit("geom_pose_palette_mutant_control");
  zhao::exit_hard(rc);
}
