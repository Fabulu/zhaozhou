// terrain_lodshare_directed.cpp -- TERRAIN.LODSHARE, the PREPARE/EMIT
// time-share over the single `zhao_terrain_lod` instance.
//
// Contract: design/contracts/TERRAIN.EDGERECON.md, "THE REMAINDER, AS FOUR
// PACKETS", row P3. Law: reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt
// section 2 -- "time-share the existing LOD machinery", "Freeze the admitted
// set, its epoch, relevant camera/governor inputs ... across PREPARE and EMIT",
// "PREPARE must not commit hysteresis history or advance the state EMIT treats
// as last frame."
//
// WHAT THIS GUARDS, and every one of these is silent in a result-only test:
//
//   * THE PATCH COORDINATE SLIPPING AGAINST THE LADDER'S ANSWER. `zhao_terrain_
//     lod` carries `src_id` and NO coordinate, so the coordinate must be held
//     beside the ladder -- and a plain "the patch we are on" register pairs
//     patch A's levels with patch B's coordinate on any stall, with every
//     accepted/emitted counter balancing. Case 3 stalls the file port in the
//     middle of a patch and requires the coordinate that comes out to be the
//     one that went in WITH that descriptor.
//   * A HISTORY WRITEBACK ESCAPING DURING PREPARE. EMIT reads history at
//     `R_HREQ` BEFORE its own ladder runs, so a PREPARE-phase write is read
//     next as "the previous frame's level" -- a hysteresis corruption with
//     every counter balancing. Case 5 offers history in both phases and
//     requires it forwarded in one and swallowed in the other.
//   * A GOVERNOR TARGET MOVING BETWEEN THE TWO PASSES. Case 4 moves all
//     thirteen live inputs mid-pass and requires LOD's inputs not to move.
//   * THE IDLE CLIENT'S READY FLOATING HIGH, which retires descriptors into
//     nothing -- the quietest way to lose a patch. Case 2 checks both readys
//     are low for the client that does not own the ladder.
//
// EVERY COUNTER IS FIRED ON PURPOSE, WITH A CONTROL BESIDE IT, except the one
// that cannot be reached with legal stimulus:
//
//   prep_descriptors_o   case 1
//   emit_descriptors_o   case 2
//   prep_decisions_o     case 1
//   emit_decisions_o     case 2
//   prep_underside_o     case 6   the underside replay is forwarded, not dropped
//   ident_mismatch_o     case 7   the ladder returns a src_id nobody queued
//   hist_leak_o          case 5   history OFFERED during PREPARE
//   sel_midpatch_o       case 8   the selector moves with answers in flight
//   freeze_drift_o       case 4   the live camera moved during a pass
//   freezes_o            case 1
//   idq_full_stalls_o    case 9   the queue really does reach full
//   idq_overflow_o       *** UNREACHABLE BY CONSTRUCTION ***
//
// `idq_overflow_o` counts a push into a FULL queue. `p_sp_fire_c` requires
// `lod_sp_valid_o`, which carries `!idq_full_c`, so the two are mutually
// exclusive and NO stimulus can move it. Its demonstration is the committed
// mutant `tests/mutants/zhao_terrain_lodshare_idqoverflow_mutant.sv`, driven by
// `terrain_lodshare_mutant` with INVERTED polarity. Case 9 is that mutant's
// positive control: it proves the queue reaches full in a HEALTHY block, so a
// mutant run reporting zero cannot be excused as "the state was never reached".
//
// AND CASE 4 IS THE ONE THAT MEASURES A DEFECT THE CONSOLE HAS TODAY. The
// console captures the thirteen governor targets under `else if (tld_idle)`
// (`zhao_console_core.sv:26414`), and `tld_idle` is high BETWEEN EVERY PATCH --
// so they are re-sampled ~256 times a frame. Entry I21 calls that "harmless
// TODAY" because MEASURE.GOVERNOR decides on `frame_i`. That is true of the
// seven `mgv_*` values and FALSE of the six eye values, which come from
// `zhao_view_eye` on `cfg_we_i`, "a pulse the executor owns, and a write lands
// the cycle it is". `freeze_drift_o` is the instrument for it.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_lodshare.h"

#include "zhao_sim.hpp"

namespace {

int failures = 0;
int checks = 0;

void check_eq(uint64_t got, uint64_t want, const char* what) {
  ++checks;
  if (got != want) {
    std::printf("FAIL: %s -- expected 0x%llx, got 0x%llx\n", what,
                static_cast<unsigned long long>(want),
                static_cast<unsigned long long>(got));
    ++failures;
  }
}

void check_true(bool ok, const char* what) {
  ++checks;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}

// One PREPARE descriptor, as `zhao_terrain_prepwalk` presents it.
struct Desc {
  int32_t cx = 0, cy = 0, cz = 0;
  uint32_t dev1 = 0, dev2 = 0, dev3 = 0;
  uint32_t prev_level = 0, prev_morph = 0, hold = 0;
  uint32_t src_id = 0;
  int32_t ix = 0, iz = 0;
  uint32_t gen = 0;
};

class Harness {
 public:
  Vzhao_terrain_lodshare t;

  void quiet() {
    t.frame_i = 0;
    t.prep_sel_i = 0;
    t.gv_cam0_x_i = 0; t.gv_cam0_y_i = 0; t.gv_cam0_z_i = 0;
    t.gv_cam0_scale_i = 0; t.gv_cam0_en_i = 0;
    t.gv_cam1_x_i = 0; t.gv_cam1_y_i = 0; t.gv_cam1_z_i = 0;
    t.gv_cam1_scale_i = 0; t.gv_cam1_en_i = 0;
    t.gv_hyst_i = 0; t.gv_min_hold_i = 0; t.gv_morph_step_i = 0;
    t.p_sp_valid_i = 0;
    t.p_sp_cx_i = 0; t.p_sp_cy_i = 0; t.p_sp_cz_i = 0;
    t.p_sp_dev1_i = 0; t.p_sp_dev2_i = 0; t.p_sp_dev3_i = 0;
    t.p_sp_prev_level_i = 0; t.p_sp_prev_morph_i = 0; t.p_sp_hold_i = 0;
    t.p_sp_src_id_i = 0; t.p_sp_ix_i = 0; t.p_sp_iz_i = 0; t.p_sp_gen_i = 0;
    t.e_sp_valid_i = 0;
    t.e_sp_cx_i = 0; t.e_sp_cy_i = 0; t.e_sp_cz_i = 0;
    t.e_sp_dev1_i = 0; t.e_sp_dev2_i = 0; t.e_sp_dev3_i = 0;
    t.e_sp_prev_level_i = 0; t.e_sp_prev_morph_i = 0; t.e_sp_hold_i = 0;
    t.e_sp_src_id_i = 0;
    t.lod_sp_ready_i = 1;
    t.lod_out_valid_i = 0;
    t.lod_out_ox_i = 0; t.lod_out_oz_i = 0; t.lod_out_level_i = 0;
    t.lod_out_lvl_nz_i = 0; t.lod_out_lvl_pz_i = 0;
    t.lod_out_lvl_nx_i = 0; t.lod_out_lvl_px_i = 0;
    t.lod_out_morph_i = 0; t.lod_out_surface_i = 0; t.lod_out_dual_i = 0;
    t.lod_out_src_id_i = 0; t.lod_out_hold_i = 0;
    t.f_ready_i = 1;
    t.e_out_ready_i = 1;
    t.h_valid_i = 0; t.h_level_i = 0; t.h_morph_i = 0; t.h_hold_i = 0;
    t.h_ready_i = 1;
  }

  void reset() {
    quiet();
    t.rst_n = 0;
    for (int i = 0; i < 3; ++i) zhao::tick(t);
    t.rst_n = 1;
    t.eval();
    idle(2);
  }

  void idle(int n) {
    for (int i = 0; i < n; ++i) {
      zhao::tick(t);
      t.eval();
    }
  }

  // Pulse the frame boundary, which is the ONLY enable on the freeze.
  void frame() {
    t.frame_i = 1;
    zhao::tick(t);
    t.frame_i = 0;
    t.eval();
  }

  void set_gov(int32_t base) {
    t.gv_cam0_x_i = base + 1; t.gv_cam0_y_i = base + 2; t.gv_cam0_z_i = base + 3;
    t.gv_cam0_scale_i = static_cast<uint16_t>(base + 4);
    t.gv_cam0_en_i = 1;
    t.gv_cam1_x_i = base + 5; t.gv_cam1_y_i = base + 6; t.gv_cam1_z_i = base + 7;
    t.gv_cam1_scale_i = static_cast<uint16_t>(base + 8);
    t.gv_cam1_en_i = 1;
    t.gv_hyst_i = static_cast<uint16_t>(base + 9);
    t.gv_min_hold_i = static_cast<uint8_t>(base + 10);
    t.gv_morph_step_i = static_cast<uint32_t>(base + 11);
    t.eval();
  }

  // Offer one descriptor on the PREPARE port and wait for it to be accepted.
  // Returns false if the block never took it inside the bound.
  bool offer_prep(const Desc& d, int bound = 64) {
    t.p_sp_valid_i = 1;
    t.p_sp_cx_i = d.cx; t.p_sp_cy_i = d.cy; t.p_sp_cz_i = d.cz;
    t.p_sp_dev1_i = d.dev1; t.p_sp_dev2_i = d.dev2; t.p_sp_dev3_i = d.dev3;
    t.p_sp_prev_level_i = d.prev_level;
    t.p_sp_prev_morph_i = d.prev_morph;
    t.p_sp_hold_i = d.hold;
    t.p_sp_src_id_i = d.src_id;
    t.p_sp_ix_i = d.ix; t.p_sp_iz_i = d.iz; t.p_sp_gen_i = d.gen;
    t.eval();
    for (int i = 0; i < bound; ++i) {
      if (t.p_sp_ready_o) {
        zhao::tick(t);
        t.p_sp_valid_i = 0;
        t.eval();
        return true;
      }
      zhao::tick(t);
      t.eval();
    }
    t.p_sp_valid_i = 0;
    t.eval();
    return false;
  }

  // Present one ladder answer and wait for it to be retired.
  bool answer(uint32_t src_id, uint32_t ox, uint32_t oz, uint32_t level,
              bool surface = false, int bound = 64) {
    t.lod_out_valid_i = 1;
    t.lod_out_src_id_i = src_id;
    t.lod_out_ox_i = ox;
    t.lod_out_oz_i = oz;
    t.lod_out_level_i = level;
    t.lod_out_surface_i = surface ? 1 : 0;
    t.eval();
    for (int i = 0; i < bound; ++i) {
      if (t.lod_out_ready_o) {
        zhao::tick(t);
        t.lod_out_valid_i = 0;
        t.eval();
        return true;
      }
      zhao::tick(t);
      t.eval();
    }
    t.lod_out_valid_i = 0;
    t.eval();
    return false;
  }
};

// ===========================================================================
void case1_prepare_path(Harness& h) {
  std::printf("-- case 1: a PREPARE descriptor reaches the ladder and its answer reaches the file port\n");
  h.reset();
  h.set_gov(1000);
  h.frame();
  check_eq(h.t.freezes_o, 1, "case1: the frame pulse froze once");
  // The frozen copy is what LOD sees.
  check_eq(static_cast<uint32_t>(h.t.lod_cam0_x_o), 1001, "case1: cam0_x frozen");
  check_eq(h.t.lod_min_hold_o, 1010 & 0xFF, "case1: min_hold frozen");

  h.t.prep_sel_i = 1;
  h.t.eval();

  Desc d;
  d.cx = 0x11110000; d.cy = 0x22220000; d.cz = 0x33330000;
  d.dev1 = 0x0A0A0A; d.dev2 = 0x0B0B0B; d.dev3 = 0x0C0C0C;
  d.prev_level = 2; d.prev_morph = 0x1234; d.hold = 7;
  d.src_id = 0xBEEF; d.ix = 40; d.iz = -17;
  check_true(h.offer_prep(d), "case1: the descriptor was accepted");

  // It arrived at the ladder unchanged -- this is the mux, so every field is
  // checked rather than sampled. A mux that drops one field is invisible until
  // the one frame that field mattered.
  check_eq(static_cast<uint32_t>(h.t.lod_sp_cx_o), 0x11110000u, "case1: cx to the ladder");
  check_eq(static_cast<uint32_t>(h.t.lod_sp_cy_o), 0x22220000u, "case1: cy to the ladder");
  check_eq(static_cast<uint32_t>(h.t.lod_sp_cz_o), 0x33330000u, "case1: cz to the ladder");
  check_eq(h.t.lod_sp_dev1_o, 0x0A0A0Au, "case1: dev1 to the ladder");
  check_eq(h.t.lod_sp_dev2_o, 0x0B0B0Bu, "case1: dev2 to the ladder");
  check_eq(h.t.lod_sp_dev3_o, 0x0C0C0Cu, "case1: dev3 to the ladder");
  check_eq(h.t.lod_sp_prev_level_o, 2, "case1: prev_level to the ladder");
  check_eq(h.t.lod_sp_prev_morph_o, 0x1234u, "case1: prev_morph to the ladder");
  check_eq(h.t.lod_sp_hold_o, 7, "case1: hold to the ladder");
  check_eq(h.t.lod_sp_src_id_o, 0xBEEFu, "case1: src_id to the ladder");
  check_eq(h.t.prep_descriptors_o, 1, "case1: one PREPARE descriptor counted");

  // The ladder answers. The COORDINATE must come back out with it, and it came
  // from the queue rather than from the ladder, which carries none.
  h.t.lod_out_valid_i = 1;
  h.t.lod_out_src_id_i = 0xBEEF;
  h.t.lod_out_ox_i = 8;
  h.t.lod_out_oz_i = 16;
  h.t.lod_out_level_i = 3;
  h.t.eval();
  check_true(h.t.f_valid_o != 0, "case1: the file port is offered the decision");
  check_eq(h.t.f_ix_o, 40, "case1: the patch ix came back with the answer");
  check_eq(h.t.f_iz_o, static_cast<uint16_t>(-17), "case1: the patch iz came back with the answer");
  check_eq(h.t.f_ox_o, 8, "case1: ox forwarded");
  check_eq(h.t.f_oz_o, 16, "case1: oz forwarded");
  check_eq(h.t.f_level_o, 3, "case1: level forwarded");
  check_eq(h.t.e_out_valid_o, 0, "case1: the EMIT consumer is NOT offered it");
  zhao::tick(h.t);
  h.t.lod_out_valid_i = 0;
  h.t.eval();
  check_eq(h.t.prep_decisions_o, 1, "case1: one PREPARE decision counted");
  check_eq(h.t.emit_decisions_o, 0, "case1: ... and the EMIT counter is flat beside it");
  check_eq(h.t.ident_mismatch_o, 0, "case1: the identity matched");
}

// ===========================================================================
void case2_emit_path_and_idle_ready(Harness& h) {
  std::printf("-- case 2: the EMIT path is untouched, and the IDLE client's ready is LOW\n");
  h.reset();
  h.frame();
  h.t.prep_sel_i = 0;
  h.t.e_sp_valid_i = 1;
  h.t.e_sp_cx_i = 0x44440000;
  h.t.e_sp_src_id_i = 0x00C0;
  h.t.p_sp_valid_i = 1;      // the PREPARE client offers at the same time
  h.t.p_sp_cx_i = 0x55550000;
  h.t.p_sp_src_id_i = 0x00D0;
  h.t.eval();

  // THE POINT OF THE CASE. A ready that floated high for the client which does
  // not own the ladder would retire its descriptors into nothing.
  check_eq(h.t.p_sp_ready_o, 0, "case2: the PREPARE client's ready is LOW in EMIT");
  check_true(h.t.e_sp_ready_o != 0, "case2: the EMIT client's ready is high");
  check_eq(static_cast<uint32_t>(h.t.lod_sp_cx_o), 0x44440000u,
           "case2: the EMIT descriptor is what reaches the ladder");
  check_eq(h.t.lod_sp_src_id_o, 0x00C0u, "case2: ... with the EMIT src_id");

  zhao::tick(h.t);
  h.t.e_sp_valid_i = 0;
  h.t.p_sp_valid_i = 0;
  h.t.eval();
  check_eq(h.t.emit_descriptors_o, 1, "case2: one EMIT descriptor counted");
  check_eq(h.t.prep_descriptors_o, 0, "case2: ... and the PREPARE counter is flat beside it");

  // The answer goes to JOBISSUE, with every field, and NOT to the file port.
  h.t.lod_out_valid_i = 1;
  h.t.lod_out_src_id_i = 0x00C0;
  h.t.lod_out_level_i = 1;
  h.t.lod_out_lvl_nz_i = 2; h.t.lod_out_lvl_pz_i = 3;
  h.t.lod_out_lvl_nx_i = 1; h.t.lod_out_lvl_px_i = 0;
  h.t.lod_out_morph_i = 0x777;
  h.t.lod_out_dual_i = 1;
  h.t.lod_out_hold_i = 0x5A;
  h.t.eval();
  check_true(h.t.e_out_valid_o != 0, "case2: the EMIT consumer is offered the decision");
  check_eq(h.t.f_valid_o, 0, "case2: the file port is NOT");
  // The four neighbour levels are EMIT-only and must survive the demux: they
  // are the whole reason TERRAIN.EDGERECON exists.
  check_eq(h.t.e_out_lvl_nz_o, 2, "case2: lvl_nz forwarded");
  check_eq(h.t.e_out_lvl_pz_o, 3, "case2: lvl_pz forwarded");
  check_eq(h.t.e_out_lvl_nx_o, 1, "case2: lvl_nx forwarded");
  check_eq(h.t.e_out_lvl_px_o, 0, "case2: lvl_px forwarded");
  check_eq(h.t.e_out_morph_o, 0x777u, "case2: morph forwarded");
  check_eq(h.t.e_out_dual_o, 1, "case2: dual forwarded");
  check_eq(h.t.e_out_hold_o, 0x5Au, "case2: hold forwarded");
  zhao::tick(h.t);
  h.t.lod_out_valid_i = 0;
  h.t.eval();
  check_eq(h.t.emit_decisions_o, 1, "case2: one EMIT decision counted");
}

// ===========================================================================
void case3_identity_survives_a_stall(Harness& h) {
  std::printf("-- case 3: THE COORDINATE TRACKS THE DESCRIPTOR ACROSS A STALL\n");
  // This is the metadata-swap case. Three descriptors from THREE DIFFERENT
  // patches go into the ladder before any answer comes out; the file port is
  // held closed in between. A block holding "the patch we are on" in a plain
  // register would answer all three with the LAST coordinate.
  h.reset();
  h.frame();
  h.t.prep_sel_i = 1;
  h.t.eval();

  Desc a, b, c;
  a.src_id = 0x0A; a.ix = 100; a.iz = 200;
  b.src_id = 0x0B; b.ix = 101; b.iz = 201;
  c.src_id = 0x0C; c.ix = 102; c.iz = 202;
  check_true(h.offer_prep(a), "case3: A accepted");
  check_true(h.offer_prep(b), "case3: B accepted");
  check_true(h.offer_prep(c), "case3: C accepted");
  check_eq(h.t.prep_descriptors_o, 3, "case3: three descriptors in flight");

  // Answers come back in order, with the file port stalling before each.
  const uint32_t src[3] = {0x0A, 0x0B, 0x0C};
  const uint16_t ix[3] = {100, 101, 102};
  const uint16_t iz[3] = {200, 201, 202};
  for (int k = 0; k < 3; ++k) {
    h.t.f_ready_i = 0;               // STALL
    h.t.lod_out_valid_i = 1;
    h.t.lod_out_src_id_i = src[k];
    h.t.lod_out_level_i = static_cast<uint8_t>(k);
    h.t.eval();
    for (int s = 0; s < 5; ++s) { zhao::tick(h.t); h.t.eval(); }
    char what[96];
    std::snprintf(what, sizeof what, "case3: answer %d carries ix %u through the stall", k, ix[k]);
    check_eq(h.t.f_ix_o, ix[k], what);
    std::snprintf(what, sizeof what, "case3: answer %d carries iz %u through the stall", k, iz[k]);
    check_eq(h.t.f_iz_o, iz[k], what);
    h.t.f_ready_i = 1;
    h.t.eval();
    zhao::tick(h.t);
    h.t.lod_out_valid_i = 0;
    h.t.eval();
  }
  check_eq(h.t.prep_decisions_o, 3, "case3: three decisions filed");
  check_eq(h.t.ident_mismatch_o, 0, "case3: and NO identity mismatch on the healthy path");
}

// ===========================================================================
void case4_freeze(Harness& h) {
  std::printf("-- case 4: THE FROZEN GOVERNOR TARGETS DO NOT MOVE DURING A PASS\n");
  h.reset();
  h.set_gov(1000);
  h.frame();
  h.t.prep_sel_i = 1;
  h.t.eval();

  Desc d; d.src_id = 0x11; d.ix = 5; d.iz = 6;
  check_true(h.offer_prep(d), "case4: a descriptor is in flight");

  const uint32_t before_x     = h.t.lod_cam0_x_o;
  const uint32_t before_hyst  = h.t.lod_hyst_o;
  const uint32_t before_morph = h.t.lod_morph_step_o;

  // THE HOST MOVES THE CAMERA MID-PASS. `zhao_view_eye` updates on `cfg_we_i`,
  // "a pulse the executor owns, and a write lands the cycle it is" -- so this
  // is legal stimulus, not a contrived one.
  h.set_gov(9000);
  for (int i = 0; i < 6; ++i) { zhao::tick(h.t); h.t.eval(); }

  check_eq(h.t.lod_cam0_x_o, before_x, "case4: cam0_x did NOT move");
  check_eq(h.t.lod_hyst_o, before_hyst, "case4: hyst did NOT move");
  check_eq(h.t.lod_morph_step_o, before_morph, "case4: morph_step did NOT move");
  check_true(h.t.freeze_drift_o != 0,
             "case4: freeze_drift_o FIRED -- the live camera really did move");

  // THE CONTROL. Pulse the frame and the freeze takes the new values; the
  // block is holding a value, not ignoring its input.
  h.t.f_ready_i = 1;
  check_true(h.answer(0x11, 0, 0, 0), "case4: the descriptor is retired");
  h.frame();
  check_eq(h.t.lod_cam0_x_o, 9001, "case4: the NEXT frame took the new camera");
  check_eq(h.t.freezes_o, 2, "case4: two freezes");
}

// ===========================================================================
void case5_history_suppression(Harness& h) {
  std::printf("-- case 5: THE HISTORY WRITEBACK IS SWALLOWED IN PREPARE AND FORWARDED IN EMIT\n");
  h.reset();
  h.frame();

  // EMIT first: this is the CONTROL, and it comes first on purpose. A test that
  // only showed the suppression could be passing because the port is dead.
  h.t.prep_sel_i = 0;
  h.t.h_valid_i = 1;
  h.t.h_level_i = 2;
  h.t.h_morph_i = 0x1F1;
  h.t.h_hold_i = 0x33;
  h.t.eval();
  check_true(h.t.h_valid_o != 0, "case5: EMIT forwards the history beat");
  check_eq(h.t.h_level_o, 2, "case5: level forwarded");
  check_eq(h.t.h_morph_o, 0x1F1u, "case5: morph forwarded");
  check_eq(h.t.h_hold_o, 0x33u, "case5: hold forwarded");
  check_eq(h.t.hist_leak_o, 0, "case5: hist_leak_o is silent in EMIT");
  zhao::tick(h.t);
  h.t.eval();

  // PREPARE: the same beat must NOT reach devstore.
  h.t.prep_sel_i = 1;
  h.t.eval();
  check_eq(h.t.h_valid_o, 0, "case5: PREPARE SWALLOWS the history beat");
  check_true(h.t.h_ready_o != 0,
             "case5: ... and the producer is still told it was taken, so jobissue cannot wedge");
  zhao::tick(h.t);
  h.t.eval();
  check_true(h.t.hist_leak_o != 0,
             "case5: hist_leak_o FIRED -- the counter is an instrument, not a hopeful zero");
  h.t.h_valid_i = 0;
  h.t.eval();
}

// ===========================================================================
void case6_underside_is_forwarded(Harness& h) {
  std::printf("-- case 6: the underside replay is FORWARDED to the reconciler, not dropped here\n");
  // TERRAIN.EDGERECON consumes the underside beat without filing it, and that
  // is deliberate: dropping it in this mux instead would make that block's
  // `lanes_filed_o` count beats rather than decisions, which is the check that
  // catches a lost lane.
  h.reset();
  h.frame();
  h.t.prep_sel_i = 1;
  h.t.eval();
  Desc d; d.src_id = 0x77; d.ix = 9; d.iz = 9;
  check_true(h.offer_prep(d), "case6: descriptor accepted");
  h.t.lod_out_valid_i = 1;
  h.t.lod_out_src_id_i = 0x77;
  h.t.lod_out_surface_i = 1;
  h.t.eval();
  check_true(h.t.f_valid_o != 0, "case6: the underside beat IS offered to the file port");
  check_eq(h.t.f_surface_o, 1, "case6: ... carrying surface = 1, so the reconciler can drop it");
  zhao::tick(h.t);
  h.t.lod_out_valid_i = 0;
  h.t.lod_out_surface_i = 0;
  h.t.eval();
  check_eq(h.t.prep_underside_o, 1, "case6: prep_underside_o counted it");
}

// ===========================================================================
void case7_identity_mismatch_refuses_the_file(Harness& h) {
  std::printf("-- case 7: AN UNMATCHED ANSWER IS REFUSED, NOT FILED\n");
  h.reset();
  h.frame();
  h.t.prep_sel_i = 1;
  h.t.eval();
  Desc d; d.src_id = 0x21; d.ix = 31; d.iz = 32;
  check_true(h.offer_prep(d), "case7: descriptor accepted");

  // The ladder returns an identity nobody queued.
  h.t.lod_out_valid_i = 1;
  h.t.lod_out_src_id_i = 0x99;      // not 0x21
  h.t.eval();
  check_eq(h.t.f_valid_o, 0,
           "case7: the file port is NOT offered a decision it cannot name");
  check_true(h.t.lod_out_ready_o != 0,
             "case7: ... but the ladder is still drained, so a counted fault is not a hang");
  zhao::tick(h.t);
  h.t.lod_out_valid_i = 0;
  h.t.eval();
  check_true(h.t.ident_mismatch_o != 0, "case7: ident_mismatch_o FIRED");
  check_eq(h.t.prep_decisions_o, 0, "case7: and NOTHING was filed");

  // THE CONTROL: the queued identity is still there and still answerable, so
  // the mismatch did not consume it.
  //
  // SAMPLED DURING THE BEAT, NOT AFTER IT, and the first version of this case
  // got that wrong in an instructive way. `f_ix_o` is COMBINATIONAL off the
  // identity queue's head, so it is meaningful only while `f_valid_o` is high;
  // read it after the accepting edge and the head has already advanced. The
  // test then reported ix = 101 -- a coordinate left in the queue's array by
  // case 3, because a RAM has no reset and only the POINTERS are cleared. That
  // reads exactly like a coordinate swap in the RTL and is a bench sampling
  // the wrong cycle. The block is right: a streaming port's payload is a
  // statement about the beat it rides with.
  h.t.lod_out_valid_i = 1;
  h.t.lod_out_src_id_i = 0x21;
  h.t.lod_out_level_i = 3;
  h.t.eval();
  check_true(h.t.f_valid_o != 0, "case7: the real answer IS offered to the file port");
  check_eq(h.t.f_ix_o, 31, "case7: ... with its own coordinate intact");
  check_eq(h.t.f_iz_o, 32, "case7: ... and its own iz");
  zhao::tick(h.t);
  h.t.lod_out_valid_i = 0;
  h.t.eval();
  check_eq(h.t.prep_decisions_o, 1, "case7: one decision filed");
}

// ===========================================================================
void case8_selector_may_not_move_mid_patch(Harness& h) {
  std::printf("-- case 8: moving the selector with answers in flight is COUNTED\n");
  h.reset();
  h.frame();
  h.t.prep_sel_i = 1;
  h.t.eval();
  Desc d; d.src_id = 0x31; d.ix = 1; d.iz = 2;
  check_true(h.offer_prep(d), "case8: a descriptor is in flight");
  check_eq(h.t.sel_midpatch_o, 0, "case8: silent so far");

  h.t.prep_sel_i = 0;         // the selector moves while the queue is occupied
  h.t.eval();
  zhao::tick(h.t);
  h.t.eval();
  check_true(h.t.sel_midpatch_o != 0, "case8: sel_midpatch_o FIRED");

  // THE CONTROL: the same move with the queue EMPTY is legal and silent.
  h.reset();
  h.frame();
  h.t.prep_sel_i = 1; h.t.eval(); zhao::tick(h.t); h.t.eval();
  h.t.prep_sel_i = 0; h.t.eval(); zhao::tick(h.t); h.t.eval();
  check_eq(h.t.sel_midpatch_o, 0, "case8: a selector move with an EMPTY queue is silent");
}

// ===========================================================================
// THE BLOCK'S OWN PARAMETER, NAMED ONCE. A literal here is how this case came
// to assert a depth the design no longer has.
constexpr uint32_t kIdqDepth = 16;

void case9_queue_reaches_full(Harness& h) {
  std::printf("-- case 9: THE QUEUE REALLY DOES REACH FULL (the mutant's positive control)\n");
  // Without this, a mutant run reporting `idq_overflow_o == 0` could be excused
  // as "the state was never reached" -- CLAUDE.md's "a gate that cannot reach
  // the state is not evidence about the state".
  h.reset();
  h.frame();
  h.t.prep_sel_i = 1;
  h.t.eval();

  // IDQ_DEPTH IS SIXTEEN, NOT FOUR, AND THE CHANGE IS A REPAIR RATHER THAN A
  // TUNING. Corrected 2026-09-25 (EDGECLOSE). The parameter's old default and
  // this case's old constant both rested on the block header's claim that
  // `zhao_terrain_lod` is "a sequential ladder with ONE descriptor in flight".
  // It is not: `zhao_terrain_lod.sv:671-684` accepts ALL SIXTEEN subpatches
  // before it emits any of them, because a subpatch's four interior
  // neighbours need the whole `lvl[]` array. At four the queue filled on the
  // fourth descriptor, the ladder never reached its sixteenth, and the two
  // blocks DEADLOCKED with every counter reading zero.
  //
  // THIS SUITE COULD NOT HAVE SEEN IT AND THAT IS THE LESSON: it drives a
  // ladder MODEL that emits per descriptor. Ninety-six checks passed against
  // a machine that does not exist. The deadlock was found by
  // `terrain_edge_acceptance`, which instantiates the REAL ladder.
  //
  // The case itself is unchanged in kind -- fill the queue, offer one more,
  // prove the guard refuses it -- and only the depth moved.
  for (int k = 0; k < kIdqDepth; ++k) {
    Desc d; d.src_id = static_cast<uint32_t>(0x40 + k);
    d.ix = static_cast<int32_t>(k); d.iz = static_cast<int32_t>(k);
    char what[64];
    std::snprintf(what, sizeof what, "case9: descriptor %d accepted", k);
    check_true(h.offer_prep(d), what);
  }
  check_eq(h.t.prep_descriptors_o, kIdqDepth, "case9: the queue filled");

  // The (depth+1)-th must be REFUSED.
  h.t.p_sp_valid_i = 1;
  h.t.p_sp_src_id_i = 0x40 + kIdqDepth;
  h.t.eval();
  check_eq(h.t.p_sp_ready_o, 0, "case9: the next descriptor is REFUSED -- the guard holds");
  check_eq(h.t.lod_sp_valid_o, 0, "case9: ... and nothing is offered to the ladder");
  for (int i = 0; i < 4; ++i) { zhao::tick(h.t); h.t.eval(); }
  check_true(h.t.idq_full_stalls_o != 0,
             "case9: idq_full_stalls_o FIRED -- the full state IS reached");
  check_eq(h.t.idq_overflow_o, 0,
           "case9: and idq_overflow_o is ZERO, which is the correct behaviour "
           "-- its ability to fire is the committed mutant's job");
  h.t.p_sp_valid_i = 0;
  h.t.eval();
  check_eq(h.t.prep_descriptors_o, kIdqDepth,
           "case9: the refused descriptor was never counted");
}

}  // namespace

int main(int argc, char** argv) {
  // UNBUFFERED, and it is a diagnostic rather than a style choice. A piped
  // run buffers stdout in 4 KB blocks, so a bench that blocks part way
  // through prints NOTHING and a late fault reads as a failure to start --
  // CLAUDE.md's 'buffered output lost in a crash makes a late fault look
  // like an early one'. This bench cost one wedge diagnosed that way.
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  Verilated::commandArgs(argc, argv);
  Harness h;

  case1_prepare_path(h);
  case2_emit_path_and_idle_ready(h);
  case3_identity_survives_a_stall(h);
  case4_freeze(h);
  case5_history_suppression(h);
  case6_underside_is_forwarded(h);
  case7_identity_mismatch_refuses_the_file(h);
  case8_selector_may_not_move_mid_patch(h);
  case9_queue_reaches_full(h);

  std::printf("\nterrain_lodshare_directed: %d checks, %d failure(s)\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
