// terrain_bake_v2_sheet_directed.cpp -- does TERRAIN.BAKE's PER-VERTEX LAYER-F
// depth mode dig what section 9.3 says it digs, and does its counter move?
//
// ---------------------------------------------------------------------------
// WHAT THIS IS
// ---------------------------------------------------------------------------
// OWNER RULING R194, 2026-09-20, by looking: *"Shipped is fine. Slightly
// different but not off."* That accepted the nearest-texel rim, froze the
// terrain page format at 64x64, and unblocked the layer-F reader whose ADDRESS
// GENERATOR was the contested thing. `zhao_terrain_bake_v2` gained
// `cmd_depth_sheet_i` on the strength of it -- the owner's OPTION A on entry
// I32: the block keeps its parametric disc and GAINS a second, per-vertex
// depth mode.
//
// `terrain_stampdepth_directed` proves the LAW. This proves the MODE: that the
// law is wired to the right cursor, on the right beat, into the right arm of
// the scar arithmetic, and that the disc path did not move while it happened.
//
// ---------------------------------------------------------------------------
// THE FOUR CLAIMS, AND WHY EACH NEEDS ITS OWN CASE
// ---------------------------------------------------------------------------
//  1. THE SHEET DIGS, AND IT IS THE SHEET DOING IT. Case 1 bakes with
//     `cmd_radius_i = 0` -- a radius at which the DISC law writes nothing at
//     all (the block's own B5: r2 = 0 makes `covers` false everywhere). So any
//     height that moves was moved by layer F and by nothing else. A test that
//     left the radius live could not tell the two laws apart, and would pass
//     with the whole sheet path disconnected.
//
//  2. THE ADDRESS IS AGAINST THE LIVE CURSOR. `sheet_texel_o` is combinational
//     on `vtx_vi_o`/`vtx_vj_o` while the DEPTH is combinational on a strength
//     captured one state earlier -- two independent paths through one
//     `zhao_terrain_stampdepth` instance. That is a property the block relies
//     on, so it is CHECKED on every one of the 1,089 beats rather than
//     asserted in a comment: an instance fed the registered cursor instead of
//     the live one would address the PREVIOUS vertex's texel and still produce
//     a plausible crater.
//
//  3. THE COUNTER FIRES, AND IT FIRES ONLY ON THE SHEET. `sheet_vertices_dug_o`
//     is the instrument that separates "the layer-F mode ran" from "the mode
//     was wired and read zeros" -- the two readings a silent machine allows,
//     and the ones every other counter on this block reports identically.
//     CLAUDE.md: a detector reading zero is a claim, and it is the claim to
//     check hardest. Case 3 runs the SAME sheet through a DISC record and
//     requires the counter NOT to move, which is the negative control that
//     makes case 1's positive reading mean something. The state is legally
//     reachable with stimulus, so no mutant is needed or wanted.
//
//  4. THE SHARED DOWNSTREAM IS STILL SHARED. The no_bake clamp, the height16
//     rails and the section 3.4 `meets` equality are v1 arithmetic that the
//     sheet path feeds rather than replaces. Case 4 fires the clamp ON THE
//     SHEET PATH, which is the check that the mode is a new DELTA SOURCE and
//     not a second scar pipeline. If someone later bypasses the clamp for
//     sheet records, a dig would cut through a no_bake corner and nothing else
//     in the suite would notice.
//
// AND THE DISC LAW IS NOT RE-TESTED HERE. `terrain_bake_v2_directed` holds it,
// 267 checks against `zref::terrain::bake_dig`, and it must go on passing
// unchanged -- that is the evidence that the mode is ADDITIVE. Duplicating it
// here would make two places to update and neither would be the authority.
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
constexpr int kCells = 32;
constexpr int kVerts = kLat * kLat;
constexpr int kCellCount = kCells * kCells;
constexpr uint32_t kSheetTexels = zt::kSheetEdge * zt::kSheetEdge;

// spec/qformats.md: fx16 -> height16 is rescale(x, 8), round-half-up.
int32_t to_h16(int32_t fx16) {
  return static_cast<int32_t>((static_cast<int64_t>(fx16) + 128) >> 8);
}

struct Patch {
  std::vector<int16_t> base{};    // layer A
  std::vector<int16_t> scar{};    // layer B in
  std::vector<int16_t> bottom{};  // layer C
  std::vector<uint8_t> cells{};   // layer D (empty = absent)
  bool dual = false;
};

struct Run {
  std::vector<int16_t> scar{};
  std::vector<uint8_t> touched{};
  std::vector<uint8_t> clamped{};
  std::vector<uint8_t> meets{};
  uint32_t sheet_dug_delta = 0;
  uint32_t texels_delta = 0;
  int address_faults = 0;
  int beats = 0;
  bool timed_out = false;
};

// The section 3.3 corner shadow: a vertex is shadowed if ANY of the up-to-four
// layer-D cells touching it carries the no_bake bit.
bool nobake_shadow(const Patch& p, int i, int j) {
  if (p.cells.empty()) return false;
  for (int cj = j - 1; cj <= j; ++cj)
    for (int ci = i - 1; ci <= i; ++ci) {
      if (ci < 0 || cj < 0 || ci >= kCells || cj >= kCells) continue;
      if (p.cells[static_cast<size_t>(cj) * kCells + ci] & zt::kNoBakeBit) return true;
    }
  return false;
}

// Run one whole patch bake. `sheet` is layer F; `sheet_mode` selects which law
// digs. `radius` is the disc's, left at 0 for every sheet case so the disc can
// write nothing.
Run run_bake(Vzhao_terrain_bake_v2& d, const Patch& p, const uint8_t* sheet, bool sheet_mode,
             int32_t radius, int stall_mod) {
  Run o;
  o.scar.assign(kVerts, 0);
  o.touched.assign(kVerts, 0);
  o.clamped.assign(kVerts, 0);
  o.meets.assign(kVerts, 0);
  const uint32_t base_dug = d.sheet_vertices_dug_o;
  const uint32_t base_texels = d.surface_texels_touched_o;
  const bool cells = !p.cells.empty();

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
  d.cmd_dual_i = p.dual ? 1 : 0;
  d.cmd_cells_i = cells ? 1 : 0;
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

  int step = 0;
  bool done = false;
  for (long long cyc = 0; !done; ++cyc) {
    if (cyc > 400000) {
      o.timed_out = true;
      break;
    }
    d.eval();  // settle the request addresses and the phase flags
    const int vi = d.vtx_vi_o, vj = d.vtx_vj_o;
    const size_t vk = static_cast<size_t>(vj) * kLat + vi;
    const int ci = d.cell_ci_o, cj = d.cell_cj_o;
    const size_t ck = static_cast<size_t>(cj) * kCells + ci;

    // CLAIM 2: the sheet address is against the LIVE cursor, every beat.
    if (vi < kLat && vj < kLat) {
      const uint32_t want = zt::sheet_texel_for_vertex(vi) +
                            zt::sheet_texel_for_vertex(vj) * zt::kSheetEdge;
      if (d.sheet_texel_o != want) ++o.address_faults;
    }

    const bool hold = stall_mod > 1 && ((step % stall_mod) == 0);
    const bool hold2 = stall_mod > 1 && ((step % stall_mod) == 1);
    ++step;

    d.vtx_base_i = p.base[vk];
    d.vtx_scar_i = p.scar[vk];
    d.vtx_bottom_i = p.dual ? p.bottom[vk] : 0;
    d.vtx_nobake_i = nobake_shadow(p, vi, vj) ? 1 : 0;
    // The page server delivers layer F on the SAME beat as A/B/C. Addressed by
    // the block's own `sheet_texel_o`, not by an address this driver computed:
    // a driver that recomputed it would be a second implementation of the law
    // under test and could agree with a wrong block.
    d.sheet_strength_i = sheet[d.sheet_texel_o % kSheetTexels];
    d.vtx_valid_i = hold ? 0 : 1;
    d.cell_state_i = cells ? p.cells[ck] : 0;
    d.cell_valid_i = hold ? 0 : 1;
    d.sc_ready_i = hold2 ? 0 : 1;
    d.cs_ready_i = hold2 ? 0 : 1;
    d.eval();

    if (d.vtx_valid_i && d.vtx_ready_o) ++o.beats;
    if (d.sc_valid_o && d.sc_ready_i) {
      const size_t k = static_cast<size_t>(d.sc_vj_o) * kLat + d.sc_vi_o;
      if (k < static_cast<size_t>(kVerts)) {
        o.scar[k] = static_cast<int16_t>(d.sc_scar_o);
        o.touched[k] = static_cast<uint8_t>(d.sc_touched_o);
        o.clamped[k] = static_cast<uint8_t>(d.sc_clamped_o);
        o.meets[k] = static_cast<uint8_t>(d.sc_meets_o);
      }
    }
    if (d.bake_done_o) done = true;
    zhao::tick(d);
  }
  d.eval();
  o.sheet_dug_delta = d.sheet_vertices_dug_o - base_dug;
  o.texels_delta = d.surface_texels_touched_o - base_texels;
  return o;
}

// The reference the RTL is differenced against: section 9.3's composed law fed
// into v1's scar arithmetic, operator for operator with the block.
struct RefVtx {
  int16_t scar;
  bool touched;
  bool clamped;
  bool meets;
};

RefVtx ref_vertex(const Patch& p, const uint8_t* sheet, int vi, int vj) {
  const size_t k = static_cast<size_t>(vj) * kLat + vi;
  const int32_t h_base = p.base[k];
  const int32_t h_scar = p.scar[k];
  const int32_t h_bottom = p.dual ? p.bottom[k] : 0;
  const uint8_t strength = sheet[zt::sheet_texel_for_vertex(vj) * zt::kSheetEdge +
                                 zt::sheet_texel_for_vertex(vi)];
  const bool covered = strength != 0;
  const int32_t delta = covered ? to_h16(zt::stamp_depth_at_vertex(sheet, vi, vj)) : 0;
  const int64_t scar_sum = static_cast<int64_t>(h_scar) + delta;
  const int64_t min_scar = static_cast<int64_t>(h_bottom) + 1 - h_base;
  const bool guard_on = p.dual && !p.cells.empty() && nobake_shadow(p, vi, vj);
  const bool clamp = covered && guard_on && (scar_sum < min_scar);
  const int64_t guarded = clamp ? min_scar : scar_sum;
  int32_t out;
  if (!covered)
    out = h_scar;
  else if (guarded > 32767)
    out = 32767;
  else if (guarded < -32768)
    out = -32768;
  else
    out = static_cast<int32_t>(guarded);
  const bool meets = (h_base + out) <= h_bottom;
  return RefVtx{static_cast<int16_t>(out), covered, clamp, meets};
}

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
  d.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
  zhao::tick(d);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_bake_v2 dut;
  reset_dut(dut);

  // ---- layer F: a 9 m disc at strength 128 (-3.25 m, a spell impact) -------
  static uint8_t sheet[kSheetTexels];
  int painted = 0;
  for (uint32_t tj = 0; tj < zt::kSheetEdge; ++tj)
    for (uint32_t ti = 0; ti < zt::kSheetEdge; ++ti) {
      const double wx = (2.0 * ti + 1.0) / 128.0 * 32.0;
      const double wz = (2.0 * tj + 1.0) / 128.0 * 32.0;
      const double d2 = (wx - 16.0) * (wx - 16.0) + (wz - 16.0) * (wz - 16.0);
      const bool in = d2 <= 81.0;
      sheet[tj * zt::kSheetEdge + ti] = in ? 128 : 0;
      if (in) ++painted;
    }
  ctrue(painted > 600, "the layer-F disc is actually painted -- the fixture is not empty");

  // A flat patch: base 0, no scar yet, a floor 4 m down, no cells.
  Patch flat;
  flat.base.assign(kVerts, 0);
  flat.scar.assign(kVerts, 0);
  flat.bottom.assign(kVerts, static_cast<int16_t>(-4 * 256));
  flat.dual = true;

  // ------------------------------------------------------------------ 1 ----
  // THE SHEET DIGS, WITH THE DISC DISABLED (radius 0 writes nothing at all).
  std::printf("[1] per-vertex layer-F bake, radius 0 -- only the sheet can dig\n");
  uint32_t case1_dug = 0;
  {
    const Run r = run_bake(dut, flat, sheet, /*sheet_mode=*/true, /*radius=*/0, /*stall=*/0);
    ctrue(!r.timed_out, "the sheet-mode bake completes");
    cke(0, r.address_faults, "sheet_texel_o tracked the live DIG cursor on every beat");
    int moved = 0;
    for (int vj = 0; vj < kLat; ++vj)
      for (int vi = 0; vi < kLat; ++vi) {
        const RefVtx want = ref_vertex(flat, sheet, vi, vj);
        const size_t k = static_cast<size_t>(vj) * kLat + vi;
        cke(want.scar, r.scar[k], "sheet-mode scar == stamp_depth_at_vertex rescaled to height16");
        cke(want.touched ? 1 : 0, r.touched[k], "sc_touched_o == (layer F strength != 0)");
        cke(want.meets ? 1 : 0, r.meets[k], "the section 3.4 meets equality is unchanged");
        if (r.scar[k] != 0) ++moved;
      }
    // ANTI-VACUITY. A 9 m disc on a 33x33 lattice must move roughly 250
    // vertices; on an empty sheet every check above passes on zeros.
    ctrue(moved > 200 && moved < 320, "the sheet actually dug about a quarter of the lattice");
    // The shipped table's entry 8 is -3.25 m; in height16 that is
    // rescale(-212992, 8) = -832. Asserted as a NUMBER because it is the one
    // place the whole chain -- table, interpolation, rescale, scar add -- is
    // visible as a single value a person can check by hand.
    cke(-832, r.scar[static_cast<size_t>(16) * kLat + 16],
        "the crater centre is exactly -832 height16 = -3.25 m");
    case1_dug = r.sheet_dug_delta;
    std::printf("    vertices moved: %d   sheet_vertices_dug_o: %u\n", moved, case1_dug);
  }

  // ------------------------------------------------------------------ 2 ----
  // THE COUNTER FIRED, and it counted the right thing.
  std::printf("[2] sheet_vertices_dug_o FIRED\n");
  ctrue(case1_dug > 200 && case1_dug < 320,
        "sheet_vertices_dug_o moved by the number of covered vertices -- the counter FIRES");

  // ------------------------------------------------------------------ 3 ----
  // THE NEGATIVE CONTROL: the same sheet, presented to a DISC record.
  std::printf("[3] the same sheet through a DISC record leaves the counter alone\n");
  {
    const Run r = run_bake(dut, flat, sheet, /*sheet_mode=*/false,
                           /*radius=*/6 << 16, /*stall=*/0);
    ctrue(!r.timed_out, "the disc-mode bake completes");
    cke(0, r.sheet_dug_delta,
        "a disc record does not move sheet_vertices_dug_o however full the sheet is");
    // ...and it DID dig, so the zero above is a negative control and not an
    // idle machine.
    ctrue(r.texels_delta > 0, "the disc record still dug -- the control is not vacuous");
    std::printf("    disc record touched %u vertices, sheet counter delta %u\n", r.texels_delta,
                r.sheet_dug_delta);
  }

  // ------------------------------------------------------------------ 4 ----
  // THE SHARED DOWNSTREAM: the no_bake clamp fires ON THE SHEET PATH.
  std::printf("[4] the section 3.3 no_bake clamp fires on a sheet-mode dig\n");
  {
    Patch guarded = flat;
    // A shallow floor, so the -3.25 m dig would cut through it...
    guarded.bottom.assign(kVerts, static_cast<int16_t>(-1 * 256));
    // ...and a no_bake block in the middle of the crater, so the clamp is armed
    // there and nowhere else.
    guarded.cells.assign(kCellCount, 0);
    for (int cj = 12; cj < 20; ++cj)
      for (int ci = 12; ci < 20; ++ci)
        guarded.cells[static_cast<size_t>(cj) * kCells + ci] = zt::kNoBakeBit;

    const Run r = run_bake(dut, guarded, sheet, /*sheet_mode=*/true, /*radius=*/0, /*stall=*/0);
    ctrue(!r.timed_out, "the guarded sheet-mode bake completes");
    int clamps = 0;
    for (int vj = 0; vj < kLat; ++vj)
      for (int vi = 0; vi < kLat; ++vi) {
        const RefVtx want = ref_vertex(guarded, sheet, vi, vj);
        const size_t k = static_cast<size_t>(vj) * kLat + vi;
        cke(want.scar, r.scar[k], "the clamped sheet dig matches the reference exactly");
        cke(want.clamped ? 1 : 0, r.clamped[k], "sc_clamped_o agrees with the reference");
        if (r.clamped[k]) ++clamps;
      }
    ctrue(clamps > 0, "the no_bake clamp ACTUALLY FIRED on the sheet path");
    std::printf("    clamped vertices: %d\n", clamps);
  }

  // ------------------------------------------------------------------ 5 ----
  // UNDER BACKPRESSURE. The sheet path skips the stencil divide, so it reaches
  // StEmit from StVtx in one state -- a shorter path through the handshake than
  // anything the disc law produces, and the one most likely to drop a beat.
  std::printf("[5] the same sheet bake under rolling backpressure\n");
  {
    const Run r = run_bake(dut, flat, sheet, /*sheet_mode=*/true, /*radius=*/0, /*stall=*/3);
    ctrue(!r.timed_out, "the stalled sheet-mode bake completes");
    cke(0, r.address_faults, "sheet_texel_o tracks the cursor under backpressure too");
    for (int vj = 0; vj < kLat; ++vj)
      for (int vi = 0; vi < kLat; ++vi) {
        const RefVtx want = ref_vertex(flat, sheet, vi, vj);
        cke(want.scar, r.scar[static_cast<size_t>(vj) * kLat + vi],
            "backpressure changes no height");
      }
    cke(static_cast<long long>(case1_dug), static_cast<long long>(r.sheet_dug_delta),
        "and the same number of vertices is dug -- no beat was counted twice or lost");
  }

  if (g_fail == 0) {
    std::printf(
        "PASS terrain_bake_v2_sheet_directed -- %lld checks. Option A's "
        "per-vertex layer-F mode digs zref's law exactly, addresses the live "
        "cursor, shares v1's clamp and rails, survives backpressure; "
        "sheet_vertices_dug_o FIRED at %u and stayed at 0 on a disc record.\n",
        g_checks, case1_dug);
    zhao::exit_hard(0);
  }
  std::printf("FAILED terrain_bake_v2_sheet_directed -- %d of %lld check(s)\n", g_fail, g_checks);
  zhao::exit_hard(1);
}
