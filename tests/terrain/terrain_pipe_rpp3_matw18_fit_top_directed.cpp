// terrain_pipe_rpp3_matw18_fit_top_directed.cpp -- the G8B wrapper's legal-mask
// activity witness.
//
// The receipt gate for G8B wants "legal masks 01/10/11, headline dense two-view
// counters, exact owner-sealed AUX identity, one shared projector, all
// simulation gates, then one clean G8B receipt proving the parameters actually
// elaborated". This is the simulation side of that: it runs the wrapper and
// asserts the traffic it generates is REAL.
//
// A fit wrapper that compiles, meets timing and drives nothing is the easiest
// possible way to produce a confident meaningless number -- the whole design
// optimises to a constant and the ALM count describes an empty box. So every
// check below is about the machine having actually done something, and the two
// that matter most are the ones that would be silent if it had not:
//
//   * all three legal masks were exercised, not just whichever one the rotor
//     happened to stop on;
//   * mat_refused_o is zero, so the projector ran on a matrix it actually
//     loaded rather than on reset values.
//
// ENFORCED-BY: tests/CMakeLists.txt terrain_pipe_rpp3_matw18_fit_top_directed.

#include <cstdint>
#include <cstdio>

// Emptied BEFORE the Verilator headers, or svdpi declares svSetScope and
// svGetScopeFromName as DLL imports and the link fails on __imp_ symbols.
// raster_texture_v3_fit_top_directed.cpp carries the same two lines for the
// same reason.
#define DPI_DLLISPEC
#define PLI_DLLISPEC

#include "verilated.h"
#include "svdpi.h"
#include "Vzhao_terrain_pipe_rpp3_matw18_fit_top.h"
#include "Vzhao_terrain_pipe_rpp3_matw18_fit_top__Dpi.h"
#include "zhao_sim.hpp"

namespace {

constexpr int kResetClocks = 8;
constexpr int kRunClocks = 40000;

struct Activity {
  uint32_t cfg_writes = 0;
  uint32_t jobs_offered = 0;
  uint32_t jobs_accepted = 0;
  uint32_t out_words = 0;
  uint32_t out_stalls = 0;
  uint32_t tess_vertices = 0;
  uint32_t replay_triangles = 0;
  uint32_t a_grants = 0;
  uint32_t b_grants = 0;
  uint32_t contended = 0;
  uint32_t mat_refused = 0;
  uint8_t masks_seen = 0;
  bool saw_dense_output = false;
  bool arena_fault = false;
  bool idle = false;
};

void tick(Vzhao_terrain_pipe_rpp3_matw18_fit_top& dut) {
  dut.clk = 0;
  dut.eval();
  dut.clk = 1;
  dut.eval();
}

Activity sample() {
  Activity a;
  // masks_seen_o is a 3-bit vector, so it arrives as svBitVecVal and not svBit.
  svBitVecVal masks = 0;
  svBit dense = 0, fault = 0, idle = 0;
  zhao_g8b_get_activity(
      reinterpret_cast<unsigned int*>(&a.cfg_writes),
      reinterpret_cast<unsigned int*>(&a.jobs_offered),
      reinterpret_cast<unsigned int*>(&a.jobs_accepted),
      reinterpret_cast<unsigned int*>(&a.out_words), reinterpret_cast<unsigned int*>(&a.out_stalls),
      reinterpret_cast<unsigned int*>(&a.tess_vertices),
      reinterpret_cast<unsigned int*>(&a.replay_triangles),
      reinterpret_cast<unsigned int*>(&a.a_grants), reinterpret_cast<unsigned int*>(&a.b_grants),
      reinterpret_cast<unsigned int*>(&a.contended),
      reinterpret_cast<unsigned int*>(&a.mat_refused), &masks, &dense, &fault, &idle);
  a.masks_seen = static_cast<uint8_t>(masks);
  a.saw_dense_output = dense != 0;
  a.arena_fault = fault != 0;
  a.idle = idle != 0;
  return a;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_pipe_rpp3_matw18_fit_top dut;

  dut.rst_n = 0;
  for (int i = 0; i < kResetClocks; ++i) tick(dut);
  dut.rst_n = 1;

  // The signature must MOVE. A fit top whose only outputs are constant is a
  // design the fitter is free to delete, and the resulting ALM number would be
  // about nothing.
  uint8_t first_signature = dut.fit_signature_o;
  bool signature_moved = false;
  uint32_t epoch_seen = 0;

  for (int i = 0; i < kRunClocks; ++i) {
    tick(dut);
    if (dut.fit_signature_o != first_signature) signature_moved = true;
    if (dut.fit_epoch_o < 32) epoch_seen |= (1u << dut.fit_epoch_o);
  }

  // The exported task reads the wrapper's own registers, so the DPI scope has
  // to be the wrapper instance before it is called.
  const svScope scope = svGetScopeFromName("TOP.zhao_terrain_pipe_rpp3_matw18_fit_top");
  zhao::check(scope != nullptr, "the wrapper's DPI scope resolves", 1, scope != nullptr ? 1 : 0);
  if (scope == nullptr) {
    return zhao::report_and_exit("terrain_pipe_rpp3_matw18_fit_top_directed");
  }
  svSetScope(scope);

  const Activity a = sample();

  std::printf(
      "[g8b_fit_top] cfg=%u jobs=%u/%u out=%u stalls=%u tess=%u replay=%u\n"
      "[g8b_fit_top] a_grants=%u b_grants=%u contended=%u refused=%u masks=%u\n",
      a.cfg_writes, a.jobs_accepted, a.jobs_offered, a.out_words, a.out_stalls, a.tess_vertices,
      a.replay_triangles, a.a_grants, a.b_grants, a.contended, a.mat_refused, a.masks_seen);

  // --- the configuration actually completed --------------------------------
  zhao::check(a.cfg_writes == 36, "wrapper writes both views' 18 config words", 36, a.cfg_writes);

  // --- THE CHECK THIS WRAPPER EXISTS FOR ------------------------------------
  // At MATW=18 a product word that does not fit is refused and the register
  // keeps its old value. A nonzero count here means the fit would characterise
  // a projector running on reset values.
  zhao::check(a.mat_refused == 0, "MATW=18 loaded every product word without a refusal", 0,
              a.mat_refused);

  // --- legal-mask activity witness -----------------------------------------
  zhao::check(a.masks_seen == 0x7, "all three legal view masks 01/10/11 were accepted", 0x7,
              a.masks_seen);

  // --- the machine did work -------------------------------------------------
  zhao::check(a.jobs_accepted > 0, "terrain jobs were accepted", 1, a.jobs_accepted > 0 ? 1 : 0);
  zhao::check(a.tess_vertices > 0, "the tessellator produced vertices", 1,
              a.tess_vertices > 0 ? 1 : 0);
  zhao::check(a.replay_triangles > 0, "replay produced triangles", 1,
              a.replay_triangles > 0 ? 1 : 0);
  zhao::check(a.out_words > 0, "triangles were accepted at the output", 1, a.out_words > 0 ? 1 : 0);

  // --- BOTH clients used the ONE shared projector ---------------------------
  // a_grants is the geometry stream, b_grants the terrain fills. If either is
  // zero the wrapper is exercising half the subsystem, and `contended` firing
  // is what proves they are sharing rather than taking turns by luck.
  zhao::check(a.a_grants > 0, "the geometry client used the shared projector", 1,
              a.a_grants > 0 ? 1 : 0);
  zhao::check(a.b_grants > 0, "the terrain client used the shared projector", 1,
              a.b_grants > 0 ? 1 : 0);
  zhao::check(a.contended > 0, "shared-projector contention was seen to fire", 1,
              a.contended > 0 ? 1 : 0);

  // --- backpressure is in the measured circuit ------------------------------
  zhao::check(a.out_stalls > 0, "the output sink stalled, so held-output logic is exercised", 1,
              a.out_stalls > 0 ? 1 : 0);

  // --- nothing illegal happened --------------------------------------------
  zhao::check(!a.arena_fault, "legal traffic produced no arena fill fault", 0,
              a.arena_fault ? 1 : 0);

  // --- the pins are alive ---------------------------------------------------
  zhao::check(signature_moved, "fit_signature_o changes during the run", 1,
              signature_moved ? 1 : 0);
  zhao::check(epoch_seen == 0xFFFFFFFFu, "fit_epoch_o visits all 32 low signature sources", 1,
              epoch_seen == 0xFFFFFFFFu ? 1 : 0);

  return zhao::report_and_exit("terrain_pipe_rpp3_matw18_fit_top_directed");
}
