// bake_delta_idempotence_directed.cpp -- OWNER RULING R231: the same stamp
// issued twice must dig ONCE.
//
// ---------------------------------------------------------------------------
// WHY A COUNTER IS NOT THE ANSWER, AND WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// `zhao_terrain_bake_v2` ACCUMULATES: `scar_sum = h_scar + delta16`. Until
// 2026-09-21 its layer-F arm fed that accumulator an ABSOLUTE depth --
// `zref::terrain::stamp_depth_at_vertex`, a function of the CURRENT sheet
// strength alone. An absolute depth added to an accumulating scar DOUBLE-DIGS:
// a stamp re-issued at the same place digs the full depth a second time, and
// two stamps overlapping inside one frame both dig the already-accumulated
// sheet.
//
// `design/contracts/SURFACE.STAMP.md` decision S3 had ruled it from the
// beginning, and rejected the built branch BY NAME: "`stamp_results` carries
// {texel, tag, strength_after, strength_before}. TERRAIN.BAKE ... NEEDS THE
// DELTA, NOT JUST THE NEW VALUE; sending `before` costs eight wires and SAVES
// BAKE A SECOND READ PORT ONTO THE SHEET. *Rejected:* emitting only the new
// value and letting BAKE re-read." Owner ruling R231 took the delta.
//
// **NO COUNTER IN THE MACHINE COULD SEE THE DEFECT.** `sheet_vertices_dug_o`
// here, and `fallbacks_o` / `miss_texels_o` / `prefetch_beats_o` in the seam,
// ALL describe a perfectly healthy read of a sheet that is telling the truth.
// The fault was in WHICH QUESTION WAS ASKED of it, and every instrument
// measured the answer. That is `CLAUDE.md`'s metadata-bank law in its purest
// form -- "counters that balance perfectly because none of them looks at the
// field that moved" -- and it is why adding a tenth counter would have been
// the wrong repair. The instrument that CAN see this class of fault is a
// DIFFERENTIAL against the oracle across TWO bakes, which is what R215 found
// for PAGEIO's three defects "no counter could see".
//
// So this file does not read a counter to decide anything. It bakes, carries
// the scar forward exactly as the page would, bakes again, and differences.
//
// ---------------------------------------------------------------------------
// THE SIX CASES, AND WHAT EACH ONE ALONE WOULD MISS
// ---------------------------------------------------------------------------
//  1. THE DELTA IS THE LAW, AND A FIRST BAKE DID NOT MOVE. One bake against a
//     `before` plane of zeroes. Checked against `stamp_delta_at_vertex` AND
//     against the old `stamp_depth_at_vertex` -- the two must AGREE here,
//     because `kStampDepthTable[0]` is 0. That equality is the whole
//     no-regression argument for `terrain_bake_v2_sheet_directed`'s 6,548
//     checks, and it is asserted rather than asserted-about.
//
//  2. IDEMPOTENCE -- THE HEART. Bake once from a fresh sheet, carry the scar,
//     then re-issue THE SAME STAMP. ABI operation 0 is `max(dst, src)`
//     (`zref::surface::blend_of_abi_operation`, and SURFACE.STAMP's S1), so a
//     repeat of an identical stamp leaves `before == after` at every texel.
//     The scar must be BYTE-IDENTICAL after the second bake. Under the
//     absolute law it doubled.
//
//  3. THE POSITIVE CONTROL FOR CASE 2, and case 2 is worthless without it.
//     A test that passes because the machine is INERT looks exactly like a
//     test that passes because the machine is CORRECT. So the second bake is
//     re-run with the `before` plane forced to zero -- which is precisely what
//     the absolute law fed it -- and the scar is required to DOUBLE. If that
//     check ever stops firing, case 2 has stopped meaning anything.
//     (`CLAUDE.md`: a detector that has not been shown to FIRE has not been
//     tested. This fires the TEST, not a counter.)
//
//  4. THE DEFERRAL IDENTITY, spec/terrain_rules.md 9.2 item 3: "applying
//     from->mid then mid->to == from->to, so a deferred patch takes one larger
//     step at its next bake." Two bakes stepping 0 -> mid -> to must leave the
//     same scar as one bake stepping 0 -> to. The delta law telescopes and the
//     absolute law has NO such identity -- which is what 9.2's "written in
//     `from`/`to` depths AND IN NOTHING ELSE" was recording, and what R231
//     means by "it restores 9.2's deferral identity". With
//     `BAKE_PATCH_BUDGET = 64` and a carry-over FIFO, this is not decorative:
//     it is the state-exactness argument for every deferred bake.
//
//  5. A PARTIAL RE-STAMP DIGS ONLY THE INCREMENT. Idempotence alone is
//     satisfiable by a block that digs nothing on ANY second bake. Raising the
//     strength must move the scar by `d(after) - d(before)` and not by
//     `d(after)`, which separates "correctly zero" from "always zero".
//
//  6. THE DISC LAW DID NOT MOVE. A `cmd_depth_sheet_i` record is the only
//     thing R231 touched. A disc record carrying the same sheet must produce
//     the same scar it always did and must not move `sheet_vertices_dug_o`.
//     `terrain_bake_v2_directed`'s 267 checks hold the disc law itself; this
//     is the check that the SHEET arm cannot leak into it.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_bake_v2.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"
#include "zref/zref_terrain_page.hpp"

namespace zt = zref::terrain;

namespace {

int g_fail = 0;
long long g_checks = 0;

void cke(long long want, long long got, const char* what) {
  ++g_checks;
  if (want != got) {
    ++g_fail;
    if (g_fail <= 20)
      std::printf("FAIL: %s -- expected %lld, got %lld\n", what, want, got);
  }
}

void ctrue(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    if (g_fail <= 20) std::printf("FAIL: %s\n", what);
  }
}

constexpr int kLat = 33;
constexpr int kVerts = kLat * kLat;
constexpr uint32_t kSheetTexels = zt::kSheetEdge * zt::kSheetEdge;

// spec/qformats.md: fx16 -> height16 is rescale(x, 8), round-half-up.
int32_t to_h16(int32_t fx16) {
  return static_cast<int32_t>((static_cast<int64_t>(fx16) + 128) >> 8);
}

// THE CONVERSION HAPPENS PER LOOKUP, AND THAT IS NOT AN ACCIDENT.
// `zhao_terrain_stampdepth` emits `depth_h16_o` -- already height16 -- and the
// block differences THOSE: `sd_delta_h16 = sd_depth_h16 - sd_before_h16`. So
// the law rounds TWICE, once per lookup, rather than once on an fx16
// difference. It matters, and the deferral identity is why:
//
//     h(d(mid)) - h(d(0)) + h(d(to)) - h(d(mid))  ==  h(d(to)) - h(d(0))
//
// telescopes EXACTLY, for any table and any rounding rule, because every
// intermediate term cancels as an exact integer. Rounding the fx16 difference
// once instead would make each step carry its own rounding error and the
// identity would hold only approximately -- so a bake deferred by
// BAKE_PATCH_BUDGET would drift from an undeferred one, which is precisely
// what spec/terrain_rules.md 9.2 item 3 forbids.
//
// This driver therefore differences two `to_h16` results and NOT
// `to_h16(stamp_delta_at_vertex(...))`. The two differ by an LSB at some
// strengths, and taking the convenient one would have been a test agreeing
// with itself.
int32_t delta_h16(const uint8_t* after, const uint8_t* before, int vi, int vj) {
  return to_h16(zref::terrain::stamp_depth_at_vertex(after, vi, vj)) -
         to_h16(zref::terrain::stamp_depth_at_vertex(before, vi, vj));
}

int16_t sat_h16(int64_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return static_cast<int16_t>(v);
}

struct Run {
  std::vector<int16_t> scar{};
  uint32_t sheet_dug_delta = 0;
  bool timed_out = false;
};

// One whole patch bake. `after` and `before` are the two layer-F strength
// planes the seam serves; `sheet_mode` selects which law digs. The disc radius
// is 0 on every sheet case, so `covers` is false everywhere and ANY height
// that moves was moved by layer F and by nothing else (the block's own B5).
Run run_bake(Vzhao_terrain_bake_v2& d, const std::vector<int16_t>& base,
             const std::vector<int16_t>& scar_in, const uint8_t* after, const uint8_t* before,
             bool sheet_mode, int32_t radius) {
  Run o;
  o.scar.assign(kVerts, 0);
  const uint32_t base_dug = d.sheet_vertices_dug_o;

  d.frame_start_i = 1;
  zhao::tick(d);
  d.frame_start_i = 0;

  d.cmd_valid_i = 1;
  d.cmd_patch_id_i = 7;
  d.cmd_cx_i = 16 << 16;
  d.cmd_cz_i = 16 << 16;
  d.cmd_radius_i = radius;
  d.cmd_depth_from_i = -(3 << 16);
  d.cmd_depth_to_i = 0;
  d.cmd_env_x0_i = 0;
  d.cmd_env_z0_i = 0;
  d.cmd_env_x1_i = 32 << 16;
  d.cmd_env_z1_i = 32 << 16;
  d.cmd_dual_i = 0;
  d.cmd_cells_i = 0;
  d.cmd_depth_sheet_i = sheet_mode ? 1 : 0;
  d.cmd_src_id_i = 0x1234;
  d.eval();
  int guard = 0;
  while (!d.cmd_ready_o) {
    zhao::tick(d);
    d.eval();
    if (++guard > 4096) {
      o.timed_out = true;
      d.cmd_valid_i = 0;
      return o;
    }
  }
  zhao::tick(d);
  d.cmd_valid_i = 0;

  bool done = false;
  for (long long cyc = 0; !done; ++cyc) {
    if (cyc > 400000) {
      o.timed_out = true;
      break;
    }
    d.eval();
    const int vi = d.vtx_vi_o, vj = d.vtx_vj_o;
    const size_t vk = static_cast<size_t>(vj) * kLat + vi;

    d.vtx_base_i = base[vk];
    d.vtx_scar_i = scar_in[vk];
    d.vtx_bottom_i = 0;
    d.vtx_nobake_i = 0;
    // The page server delivers layer F on the SAME beat as A/B/C, addressed by
    // the BLOCK's own `sheet_texel_o` -- never by an address this driver
    // computed. A driver that recomputed the address would be a second
    // implementation of the law under test, and could agree with a wrong
    // block. Both planes are read at that one address, which is also the
    // structural statement that `before` and `after` describe the SAME texel.
    d.sheet_strength_i = after[d.sheet_texel_o % kSheetTexels];
    d.sheet_before_i = before[d.sheet_texel_o % kSheetTexels];
    d.vtx_valid_i = 1;
    d.cell_state_i = 0;
    d.cell_valid_i = 1;
    d.sc_ready_i = 1;
    d.cs_ready_i = 1;
    d.eval();

    if (d.sc_valid_o && d.sc_ready_i) {
      const size_t k = static_cast<size_t>(d.sc_vj_o) * kLat + d.sc_vi_o;
      if (k < static_cast<size_t>(kVerts)) o.scar[k] = static_cast<int16_t>(d.sc_scar_o);
    }
    if (d.bake_done_o) done = true;
    zhao::tick(d);
  }
  d.eval();
  o.sheet_dug_delta = d.sheet_vertices_dug_o - base_dug;
  return o;
}

// The reference: 9.3's DELTA law fed into v1's scar arithmetic, operator for
// operator with the block. No clamp and no bottom here -- those are
// `terrain_bake_v2_sheet_directed`'s business and are not what R231 moved.
int16_t ref_scar(const std::vector<int16_t>& scar_in, const uint8_t* after, const uint8_t* before,
                 int vi, int vj) {
  const size_t k = static_cast<size_t>(vj) * kLat + vi;
  const uint32_t ti = zt::sheet_texel_for_vertex(vi);
  const uint32_t tj = zt::sheet_texel_for_vertex(vj);
  const uint8_t s_after = after[tj * zt::kSheetEdge + ti];
  // `v_covered` on the sheet path is `sheet_strength_i != 0` -- the sheet's own
  // coverage statement, unchanged by R231. R231 moved the DELTA, not the
  // coverage test, and the difference matters: a vertex that is covered but
  // unchanged is covered and contributes zero.
  if (s_after == 0) return scar_in[k];
  const int32_t delta = delta_h16(after, before, vi, vj);
  return sat_h16(static_cast<int64_t>(scar_in[k]) + delta);
}

// A stamp: every texel inside a disc of `r` texels about the sheet's centre
// takes `strength`, blended with ABI operation 0 == max(dst, src).
void stamp_max(std::vector<uint8_t>& sheet, int r, uint8_t strength) {
  const int c = static_cast<int>(zt::kSheetEdge) / 2;
  for (int tj = 0; tj < static_cast<int>(zt::kSheetEdge); ++tj)
    for (int ti = 0; ti < static_cast<int>(zt::kSheetEdge); ++ti) {
      const int dx = ti - c, dz = tj - c;
      if (dx * dx + dz * dz > r * r) continue;
      uint8_t& t = sheet[static_cast<size_t>(tj) * zt::kSheetEdge + ti];
      if (strength > t) t = strength;
    }
}

// The generic `zhao::reset` drives `in_valid`/`in_data`, which this block does
// not have. Same shape as `terrain_bake_v2_sheet_directed`'s own reset, plus
// R231's second strength plane.
void reset_dut(Vzhao_terrain_bake_v2& d) {
  d.rst_n = 0;
  d.frame_start_i = 0;
  d.cmd_valid_i = 0;
  d.vtx_valid_i = 0;
  d.cell_valid_i = 0;
  d.sc_ready_i = 0;
  d.cs_ready_i = 0;
  d.cmd_depth_sheet_i = 0;
  d.sheet_strength_i = 0;
  d.sheet_before_i = 0;
  d.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
  zhao::tick(d);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_bake_v2 d;
  reset_dut(d);

  const std::vector<int16_t> base(kVerts, 100);
  const std::vector<int16_t> zero_scar(kVerts, 0);
  const std::vector<uint8_t> zeros(kSheetTexels, 0);

  // ---- the stamp under test ------------------------------------------------
  std::vector<uint8_t> sheet_a(kSheetTexels, 0);
  stamp_max(sheet_a, 20, 128);  // strength 128 == -3.25 m, "a spell impact"

  // =========================================================================
  // CASE 1 -- THE DELTA IS THE LAW, AND A FIRST BAKE DID NOT MOVE
  // =========================================================================
  const Run r1 = run_bake(d, base, zero_scar, sheet_a.data(), zeros.data(), true, 0);
  ctrue(!r1.timed_out, "case 1: the bake completed");
  int moved = 0;
  for (int vj = 0; vj < kLat; ++vj)
    for (int vi = 0; vi < kLat; ++vi) {
      const size_t k = static_cast<size_t>(vj) * kLat + vi;
      cke(ref_scar(zero_scar, sheet_a.data(), zeros.data(), vi, vj), r1.scar[k],
          "case 1: scar == the DELTA law against a zero `before` plane");
      // AND the same number the ABSOLUTE law produced, because
      // `stamp_depth(0)` is 0. This is the no-regression proof.
      const uint32_t ti = zt::sheet_texel_for_vertex(vi), tj = zt::sheet_texel_for_vertex(vj);
      if (sheet_a[tj * zt::kSheetEdge + ti] != 0) {
        cke(to_h16(zt::stamp_depth_at_vertex(sheet_a.data(), vi, vj)),
            delta_h16(sheet_a.data(), zeros.data(), vi, vj),
            "case 1: a cold `before` plane makes the DELTA law equal the ABSOLUTE one");
        // ... and the oracle's own fx16 delta agrees with the absolute one
        // there too, which is what pins `stamp_depth(0) == 0`.
        cke(zt::stamp_depth_at_vertex(sheet_a.data(), vi, vj),
            zt::stamp_delta_at_vertex(sheet_a.data(), zeros.data(), vi, vj),
            "case 1: stamp_delta_at_vertex collapses to stamp_depth_at_vertex when before is 0");
        ++moved;
      }
    }
  ctrue(moved > 200, "case 1: the stamp actually dug a substantial crater");
  ctrue(r1.sheet_dug_delta == static_cast<uint32_t>(moved),
        "case 1: sheet_vertices_dug_o counted exactly the covered vertices");
  std::printf("[1] the delta law digs a first bake exactly as the absolute law did (%d vertices)\n",
              moved);

  // =========================================================================
  // CASE 2 -- IDEMPOTENCE. THE SAME STAMP TWICE DIGS ONCE.
  // =========================================================================
  // Operation 0 is max(dst, src), so re-issuing an identical stamp leaves the
  // sheet EXACTLY where it was: `before == after` at every texel. The scar
  // must not move at all.
  const Run r2 = run_bake(d, base, r1.scar, sheet_a.data(), sheet_a.data(), true, 0);
  ctrue(!r2.timed_out, "case 2: the second bake completed");
  for (int k = 0; k < kVerts; ++k)
    cke(r1.scar[k], r2.scar[k], "case 2: RE-ISSUING THE SAME STAMP DIGS NOTHING");
  std::printf("[2] IDEMPOTENCE: the same stamp issued twice dug once\n");

  // =========================================================================
  // CASE 3 -- THE POSITIVE CONTROL. Case 2 must be ABLE to fail.
  // =========================================================================
  // Feed the second bake a ZERO `before` plane, which is exactly what the
  // absolute law fed it, and require the crater to DOUBLE. If this stops
  // firing, case 2 is passing on an inert machine and means nothing.
  const Run r3 = run_bake(d, base, r1.scar, sheet_a.data(), zeros.data(), true, 0);
  ctrue(!r3.timed_out, "case 3: the control bake completed");
  int doubled = 0;
  for (int vj = 0; vj < kLat; ++vj)
    for (int vi = 0; vi < kLat; ++vi) {
      const size_t k = static_cast<size_t>(vj) * kLat + vi;
      const uint32_t ti = zt::sheet_texel_for_vertex(vi), tj = zt::sheet_texel_for_vertex(vj);
      if (sheet_a[tj * zt::kSheetEdge + ti] == 0) continue;
      cke(sat_h16(2LL * r1.scar[k]), r3.scar[k],
          "case 3: with `before` forced to zero the crater DOUBLES -- the defect, on demand");
      ++doubled;
    }
  ctrue(doubled > 200, "case 3: the control moved a substantial number of vertices");
  std::printf("[3] positive control: the OLD law doubles the crater on %d vertices, so case 2 can "
              "discriminate\n",
              doubled);

  // =========================================================================
  // CASE 4 -- THE DEFERRAL IDENTITY (spec/terrain_rules.md 9.2 item 3)
  // =========================================================================
  // 0 -> mid -> to must equal 0 -> to. The delta law TELESCOPES:
  // d(mid)-d(0) + d(to)-d(mid) = d(to)-d(0). The absolute law has no such
  // identity, which is exactly what R231 means by "it restores 9.2's deferral
  // identity" -- and with BAKE_PATCH_BUDGET = 64 and a carry-over FIFO, this
  // is the state-exactness argument for every deferred bake in the machine.
  std::vector<uint8_t> sheet_mid(kSheetTexels, 0);
  stamp_max(sheet_mid, 20, 64);  // strength 64 == -1.00 m, "a footfall crater"
  std::vector<uint8_t> sheet_to(kSheetTexels, 0);
  stamp_max(sheet_to, 20, 64);
  stamp_max(sheet_to, 20, 208);  // ... deepened to -7.60 m

  const Run step_a = run_bake(d, base, zero_scar, sheet_mid.data(), zeros.data(), true, 0);
  const Run step_b = run_bake(d, base, step_a.scar, sheet_to.data(), sheet_mid.data(), true, 0);
  const Run one_step = run_bake(d, base, zero_scar, sheet_to.data(), zeros.data(), true, 0);
  ctrue(!step_a.timed_out && !step_b.timed_out && !one_step.timed_out,
        "case 4: all three bakes completed");
  for (int k = 0; k < kVerts; ++k)
    cke(one_step.scar[k], step_b.scar[k],
        "case 4: 9.2's deferral identity -- from->mid then mid->to == from->to");
  std::printf("[4] the deferral identity holds: a deferred bake takes one larger step and loses "
              "nothing\n");

  // =========================================================================
  // CASE 5 -- A PARTIAL RE-STAMP DIGS ONLY THE INCREMENT
  // =========================================================================
  // Idempotence alone is satisfiable by a block that digs nothing on any
  // SECOND bake. This separates "correctly zero" from "always zero": deepening
  // the stamp must move the scar by d(after) - d(before), and by strictly less
  // than d(after).
  int incremental = 0;
  for (int vj = 0; vj < kLat; ++vj)
    for (int vi = 0; vi < kLat; ++vi) {
      const size_t k = static_cast<size_t>(vj) * kLat + vi;
      const uint32_t ti = zt::sheet_texel_for_vertex(vi), tj = zt::sheet_texel_for_vertex(vj);
      if (sheet_to[tj * zt::kSheetEdge + ti] == 0) continue;
      const int32_t inc = delta_h16(sheet_to.data(), sheet_mid.data(), vi, vj);
      cke(sat_h16(static_cast<int64_t>(step_a.scar[k]) + inc), step_b.scar[k],
          "case 5: a deepened stamp digs the INCREMENT, not the whole depth");
      const int32_t whole = to_h16(zt::stamp_depth_at_vertex(sheet_to.data(), vi, vj));
      ctrue(inc > whole, "case 5: the increment is strictly shallower than the absolute depth");
      ++incremental;
    }
  ctrue(incremental > 200, "case 5: the incremental case covered a substantial crater");
  std::printf("[5] a deepened stamp digs only the increment, on %d vertices\n", incremental);

  // =========================================================================
  // CASE 6 -- THE DISC LAW DID NOT MOVE
  // =========================================================================
  // The only thing R231 touched is the `cmd_depth_sheet_i` arm. A disc record
  // carrying the same two planes must ignore both of them.
  const uint32_t before_dug = d.sheet_vertices_dug_o;
  const Run disc_z = run_bake(d, base, zero_scar, sheet_a.data(), zeros.data(), false, 6 << 16);
  const Run disc_s = run_bake(d, base, zero_scar, sheet_a.data(), sheet_a.data(), false, 6 << 16);
  ctrue(!disc_z.timed_out && !disc_s.timed_out, "case 6: both disc bakes completed");
  int disc_moved = 0;
  for (int k = 0; k < kVerts; ++k) {
    cke(disc_z.scar[k], disc_s.scar[k],
        "case 6: a DISC record ignores the `before` plane entirely");
    if (disc_z.scar[k] != 0) ++disc_moved;
  }
  ctrue(disc_moved > 50, "case 6: the disc record actually dug, so the check is not vacuous");
  cke(0, static_cast<long long>(d.sheet_vertices_dug_o - before_dug),
      "case 6: sheet_vertices_dug_o does NOT move on a disc record");
  std::printf("[6] the disc law is untouched: %d vertices dug, sheet counter unmoved\n",
              disc_moved);

  std::printf("bake_delta_idempotence_directed: %lld checks, %d failures\n", g_checks, g_fail);
  return g_fail == 0 ? 0 : 1;
}
