// measure_governor_thresh.cpp -- the per-camera PIXEL-ERROR THRESHOLD
// (`cam*_thresh_q8_o`), owner ruling R26, scheduled by R68.
//
// A SEPARATE SUITE rather than more cases in measure_governor_directed, for
// the reason part_state_capacity_backstop is separate: that suite's check
// count is quoted as evidence in the ledger and in run logs, and a number that
// moves whenever somebody adds a case is a number nobody can use.
//
// WHAT THIS LANE WOULD CATCH:
//
//   1. THE CONVERSION, WORKED BY HAND. `px_err` is fx16 (Q16.16) and the port
//      is S12.8, so the threshold is qformats section 3's round-half-up
//      rescale by 8 -- and NOT a truncation, NOT a shift by 16, and NOT the
//      ratio `cam*_scale_o` already carries. 2.0 px of allowed error is raw
//      131072 in and raw 512 out. Red on any of those three slips.
//   2. THE ROUNDING TIE, CONSTRUCTED. A `px_err` whose low eight bits are
//      exactly 0x80 is the one input where round-half-up and truncation
//      differ. Uniform random operands hit it with probability 2^-8 per lane,
//      so it is constructed rather than hoped for.
//   3. THE DEGRADE IS THE SAME LAW G2, READ FROM THE OTHER END. Each degrade
//      rung DOUBLES the allowed pixel error, exactly as it HALVES the ratio.
//      This is the case that catches a threshold that ignores the rung -- a
//      defect under which a degraded view coarsens its terrain and keeps its
//      creatures at full rate, which is a policy split nobody chose.
//   4. THE VOLCANO AGAIN (law G3), on the new port. View 1 degrades to its
//      bottom rung for twenty frames; view 0's THRESHOLD does not move by one
//      LSB. Charter section 9's Duo fairness sentence has to hold on every
//      output of this block, not just on the one that had a consumer first.
//   5. THE THRESHOLD IS HELD AND PUBLISHED WITH THE RATIO. `decide()`'s hold
//      check now watches it too, so a threshold that moved mid-decision -- one
//      frame's tolerance against the previous frame's rung -- is a failure.
//   6. THE RESET VALUE IS THE FINE END. A console that never issues a pixel
//      error must not hold every creature at rung 0 forever.
//   7. IT MEANS WHAT IT CLAIMS, THROUGH THE REAL LADDER. The published
//      threshold is fed to `zref::creature::lod_raw` with a real creature
//      type, and the rung it picks is checked to be the rung the pixel budget
//      implies. That is the check that makes the number mean something rather
//      than merely be self-consistent.
#include "governor_dev.hpp"

#include <cstdio>

#include "zref/zref_creature.hpp"

namespace {

using zhao::check;
using gov_test::Frame;
using gov_test::Targets;

constexpr uint32_t kOne = 1u << 16;  // fx16 unity

Frame mkframe(uint16_t proj, uint32_t px0, uint32_t px1, bool s0 = false, bool s1 = false,
              int vc = 2) {
  Frame f;
  f.cam[0].proj = proj;
  f.cam[0].px_err = px0;
  f.cam[0].starved = s0;
  f.cam[1].proj = proj;
  f.cam[1].px_err = px1;
  f.cam[1].starved = s1;
  f.view_count = vc;
  f.src_id = 0;
  return f;
}

/** The law, restated in the test rather than imported from the RTL: an
 *  fx16 tolerance rescaled to S12.8 by qformats section 3's round-half-up,
 *  with the degrade rung multiplying the tolerance by 2^deg (law G2). */
int32_t want_thresh(uint32_t px_err, int deg) {
  const uint64_t w = (static_cast<uint64_t>(px_err) << deg) + 128u;
  return static_cast<int32_t>(w >> 8);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* dutp = new Vzhao_measure_governor;  // heap + exit_hard: see zhao_sim.hpp
  Vzhao_measure_governor& dut = *dutp;

  // ---- 6. the reset value, before any frame -------------------------------
  gov_test::reset_dut(dut);
  {
    const Targets t = gov_test::read_targets(dut);
    check(t.thresh_q8[0] == 256, "reset threshold is 1.0 px, the FINE end", 256,
          static_cast<uint64_t>(t.thresh_q8[0]));
    check(t.thresh_q8[1] == 256, "...on both views", 256,
          static_cast<uint64_t>(t.thresh_q8[1]));
  }

  // ---- 1. the conversion, worked by hand ----------------------------------
  {
    // 2.0 px allowed error, fx16, on view 0; 0.5 px on view 1.
    const uint32_t px0 = 2u * kOne;        // 131072
    const uint32_t px1 = kOne / 2u;        // 32768
    bool held = true;
    const Targets t = gov_test::decide(dut, mkframe(1000, px0, px1), nullptr, &held);
    check(held, "no target moved before the publish", 1, held ? 1 : 0);
    check(t.thresh_q8[0] == 512, "2.0 fx16 px becomes 512 raw S12.8", 512,
          static_cast<uint64_t>(t.thresh_q8[0]));
    check(t.thresh_q8[1] == 128, "0.5 fx16 px becomes 128 raw S12.8", 128,
          static_cast<uint64_t>(t.thresh_q8[1]));
    // NOT the ratio, and NOT a shift by 16. Both of those are the slips a
    // "looks about right" reading would miss.
    check(static_cast<uint32_t>(t.thresh_q8[0]) != t.scale[0],
          "the threshold is not the ratio wearing a new name", 1,
          static_cast<uint32_t>(t.thresh_q8[0]) != t.scale[0]);
    check(t.thresh_q8[0] != static_cast<int32_t>(px0 >> 16),
          "and it is not a rescale by 16", 1,
          t.thresh_q8[0] != static_cast<int32_t>(px0 >> 16));
  }

  // ---- 2. the rounding tie, constructed -----------------------------------
  {
    // Low eight bits exactly 0x80: round-half-up gives one more than
    // truncation, and nothing else distinguishes them.
    const uint32_t tie = (3u << 8) | 0x80u;        // 0x380 -> 3.5 raw S12.8
    const uint32_t below = (3u << 8) | 0x7Fu;
    const Targets a = gov_test::decide(dut, mkframe(1000, tie, below));
    check(a.thresh_q8[0] == 4, "an exact half rounds UP (qformats 3)", 4,
          static_cast<uint64_t>(a.thresh_q8[0]));
    check(a.thresh_q8[1] == 3, "one LSB below it does not", 3,
          static_cast<uint64_t>(a.thresh_q8[1]));
    check(a.thresh_q8[0] == want_thresh(tie, 0), "and the law agrees",
          static_cast<uint64_t>(want_thresh(tie, 0)), static_cast<uint64_t>(a.thresh_q8[0]));
  }

  // ---- 3. the degrade DOUBLES the tolerance, exactly ----------------------
  {
    gov_test::reset_dut(dut);
    const uint32_t px = 2u * kOne;
    // Rung 0, undegraded.
    const Targets t0 = gov_test::decide(dut, mkframe(1000, px, px));
    check(t0.deg[0] == 0, "view 0 starts at rung 0", 0, t0.deg[0]);
    const int32_t base = t0.thresh_q8[0];
    check(base == want_thresh(px, 0), "rung 0 threshold", want_thresh(px, 0),
          static_cast<uint64_t>(base));
    // Climb one rung at a time -- law G4 raises `deg` the frame after a
    // starved one, so each starved frame is one rung.
    for (int rung = 1; rung <= 3; ++rung) {
      const Targets t = gov_test::decide(dut, mkframe(1000, px, px, true, true));
      char nm[96];
      std::snprintf(nm, sizeof nm, "degrade rung %d is reached", rung);
      check(t.deg[0] == rung, nm, rung, t.deg[0]);
      std::snprintf(nm, sizeof nm, "rung %d DOUBLES the allowed pixel error", rung);
      check(t.thresh_q8[0] == want_thresh(px, rung), nm,
            static_cast<uint64_t>(want_thresh(px, rung)),
            static_cast<uint64_t>(t.thresh_q8[0]));
      std::snprintf(nm, sizeof nm, "rung %d: threshold up while the ratio goes down", rung);
      check(t.thresh_q8[0] > base && t.scale[0] <= t0.scale[0], nm, 1,
            t.thresh_q8[0] > base && t.scale[0] <= t0.scale[0]);
    }
    // Exactly 2^deg, stated as a product rather than trusted from the table.
    const Targets t3 = gov_test::decide(dut, mkframe(1000, px, px, true, true));
    check(t3.deg[0] == 3, "and it stops at DEG_MAX", 3, t3.deg[0]);
    check(t3.thresh_q8[0] == 8 * 512, "rung 3 is exactly eight times rung 0's 512", 8 * 512,
          static_cast<uint64_t>(t3.thresh_q8[0]));
  }

  // ---- 4. the volcano, on the new port (law G3) ---------------------------
  {
    gov_test::reset_dut(dut);
    const uint32_t px = 3u * kOne;
    const Targets first = gov_test::decide(dut, mkframe(900, px, px));
    const int32_t v0 = first.thresh_q8[0];
    for (int i = 0; i < 20; ++i) {
      const Targets t = gov_test::decide(dut, mkframe(900, px, px, false, true));
      check(t.thresh_q8[0] == v0,
            "view 1 starving for twenty frames moves view 0's threshold by ZERO LSB",
            static_cast<uint64_t>(v0), static_cast<uint64_t>(t.thresh_q8[0]));
      check(t.deg[0] == 0, "...and view 0 stays at rung 0", 0, t.deg[0]);
    }
    const Targets last = gov_test::decide(dut, mkframe(900, px, px, false, true));
    check(last.deg[1] == 3, "while view 1 is at its bottom rung", 3, last.deg[1]);
    check(last.thresh_q8[1] == want_thresh(px, 3),
          "with its own tolerance eight times wider",
          static_cast<uint64_t>(want_thresh(px, 3)),
          static_cast<uint64_t>(last.thresh_q8[1]));
  }

  // ---- 5. the limit: zero allowed error -----------------------------------
  {
    gov_test::reset_dut(dut);
    const Targets t = gov_test::decide(dut, mkframe(1000, 0u, kOne));
    check(t.thresh_q8[0] == 0,
          "law G6's limit: zero allowed error demands the finest rung, and says so", 0,
          static_cast<uint64_t>(t.thresh_q8[0]));
    check(t.scale[0] == 0xFFFF, "...which is the same statement the ratio makes", 0xFFFF,
          t.scale[0]);
  }

  // ---- 7. IT MEANS WHAT IT CLAIMS, through the real ladder ----------------
  // The published threshold goes straight into `zref::creature::lod_raw` -- the
  // law `zhao_geom_lod` implements -- with a real creature type, and the rung
  // it picks is checked against what the pixel budget implies.
  {
    gov_test::reset_dut(dut);
    zref::creature::CreatureType type;
    type.bound_radius = 1 << 16;        // 1.00 m
    type.micro_error = 1311;            // 0.020 m, measured
    type.splat_error = 1 << 15;         // bound/2
    type.glint_error = 1 << 16;         // bound
    // A generous tolerance: 8 px of allowed error.
    const Targets loose = gov_test::decide(dut, mkframe(1000, 8u * kOne, 8u * kOne));
    check(loose.thresh_q8[0] == 8 * 256, "8.0 px of tolerance", 8 * 256,
          static_cast<uint64_t>(loose.thresh_q8[0]));
    // A creature whose bound projects to 4 px. Its glint error equals the
    // bound radius, so the glint rung's screen error IS the projected radius
    // -- 4 px, inside an 8 px budget -- and the COARSEST legal rung wins.
    const zref::creature::LodRung rung_far =
        zref::creature::lod_raw(4 * 256, loose.thresh_q8[0], type);
    check(rung_far == zref::creature::LodRung::kGlint,
          "a 4 px creature under an 8 px budget is a GLINT", 3,
          static_cast<uint64_t>(rung_far));
    // A tight tolerance: 0.25 px. Nothing but the full mesh is legal for a
    // creature that large on screen.
    gov_test::reset_dut(dut);
    const Targets tight = gov_test::decide(dut, mkframe(1000, kOne / 4u, kOne / 4u));
    check(tight.thresh_q8[0] == 64, "0.25 px of tolerance", 64,
          static_cast<uint64_t>(tight.thresh_q8[0]));
    const zref::creature::LodRung rung_near =
        zref::creature::lod_raw(200 * 256, tight.thresh_q8[0], type);
    check(rung_near == zref::creature::LodRung::kMesh,
          "a 200 px creature under a quarter-pixel budget is the full MESH", 0,
          static_cast<uint64_t>(rung_near));
    // And the DEGRADE changes that verdict, which is the whole point of
    // feeding the governor's number rather than SetView's raw one.
    gov_test::reset_dut(dut);
    gov_test::decide(dut, mkframe(1000, kOne / 4u, kOne / 4u));
    Targets deg = gov_test::decide(dut, mkframe(1000, kOne / 4u, kOne / 4u, true, true));
    for (int i = 0; i < 2; ++i)
      deg = gov_test::decide(dut, mkframe(1000, kOne / 4u, kOne / 4u, true, true));
    check(deg.deg[0] == 3, "three starved frames put view 0 at rung 3", 3, deg.deg[0]);
    check(deg.thresh_q8[0] == 8 * 64, "its tolerance is eight times wider", 8 * 64,
          static_cast<uint64_t>(deg.thresh_q8[0]));
    // A creature whose bound projects to 1000 raw (3.9 px). Its SPLAT error
    // is half the bound radius, so the splat rung costs 500 raw of screen
    // error: outside a 64 budget and inside a 512 one. The undegraded view
    // must therefore spend the micro rung on it and the degraded view must
    // not -- which is the whole reason the governor's number is fed here
    // rather than SetView's raw one.
    const zref::creature::LodRung coarser =
        zref::creature::lod_raw(1000, deg.thresh_q8[0], type);
    const zref::creature::LodRung finer = zref::creature::lod_raw(1000, 64, type);
    check(static_cast<int>(coarser) > static_cast<int>(finer),
          "a degraded view genuinely picks a COARSER rung for the same creature", 1,
          static_cast<int>(coarser) > static_cast<int>(finer));
  }

  dut.final();
  return zhao::report_and_exit("measure_governor_thresh");
}
