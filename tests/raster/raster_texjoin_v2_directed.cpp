// raster_texjoin_v2_directed.cpp — does the multi-sample join hold together?
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE EXISTS FOR
// ---------------------------------------------------------------------------
// v1 joins ONE primary sample per fragment. MATERIAL_ARCHITECTURE.md ratified
// 0..3 samples with responses keyed {record_id, sample_index}, and
// reports/Addendum made that necessary before the texture island is built.
//
// Four properties separate v2 from v1, and each is checked here rather than
// asserted in a comment:
//
//   1. READY DEPENDS ON LOCAL STORAGE ONLY. Accepting a fragment must never
//      wait on the TMU. This is the property that permits II=1, and it is the
//      one a plausible implementation gets wrong -- coupling f_ready_o to
//      tmu_ready_i works, passes an ordering test, and serialises the pipe.
//      Checked by holding tmu_ready_i LOW and requiring fragments to still be
//      accepted until storage runs out.
//
//   2. ALL REQUIRED SAMPLES BEFORE RETIREMENT. A 3-sample fragment must not
//      retire on sample 0. Checked by returning two of three and requiring
//      o_valid_o to stay low.
//
//   3. COMPLETION ORDER IS NOT RETIREMENT ORDER. Returns are delivered
//      deliberately out of order -- last fragment first, samples reversed --
//      and output must still be in allocation order.
//
//   4. A STALE GENERATION IS REJECTED. This is the one that matters for slot
//      reuse: a response arriving after its slot has been recycled must be
//      DROPPED and counted, not written into the new occupant. Checked by
//      replaying a return with the wrong generation.
//
// And one that is not a property but a guard: the combiner is UNFROZEN, so a
// non-passthrough recipe must raise combiner_unfrozen_o. A gate that ignores
// that flag is testing a placeholder, and this file refuses to be that gate.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"

#include "Vzhao_raster_texjoin_v2.h"

#include "zhao_sim.hpp"

namespace {

constexpr int kDepth = 16;

struct Frag {
  int count;              // sample_count 0..3
  uint32_t u[3], v[3];
  int recipe;
  uint64_t ctx;
  bool aux;
};

void idle_inputs(Vzhao_raster_texjoin_v2& t) {
  t.f_valid_i = 0;
  t.tmu_ready_i = 1;
  t.tmu_rvalid_i = 0;
  t.aux_ready_i = 1;
  t.aux_rvalid_i = 0;
  t.o_ready_i = 1;
}

void reset(Vzhao_raster_texjoin_v2& t) {
  idle_inputs(t);
  t.rst_n = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(t);
  t.rst_n = 1;
  zhao::tick(t);
}

// Offer one fragment; returns true if it was accepted this cycle.
bool offer(Vzhao_raster_texjoin_v2& t, const Frag& f) {
  t.f_valid_i = 1;
  t.f_sample_count_i = f.count;
  for (int j = 0; j < 3; ++j) {
    t.f_u_i[j] = f.u[j];
    t.f_v_i[j] = f.v[j];
    t.f_binding_i[j] = static_cast<uint8_t>(j + 1);
    t.f_lod_i[j] = 0;
  }
  t.f_recipe_i = f.recipe;
  t.f_ctx_i = f.ctx;
  t.f_aux_i = f.aux;
  t.f_uv_sat_i = 0;
  t.eval();
  const bool took = t.f_ready_o != 0;
  zhao::tick(t);
  t.f_valid_i = 0;
  return took;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_raster_texjoin_v2 top;

  // --------------------------------------------------------------- 1 ---
  // READY IS LOCAL. Hold the TMU's ready LOW for the whole test and require
  // that fragments are still accepted -- exactly DEPTH of them, then no more.
  {
    reset(top);
    top.tmu_ready_i = 0;   // the TMU never accepts anything
    top.o_ready_i = 1;
    int accepted = 0;
    for (int i = 0; i < kDepth + 4; ++i) {
      Frag f{1, {100u + i, 0, 0}, {200u + i, 0, 0}, 0, 0xA000u + i, false};
      if (offer(top, f)) ++accepted;
    }
    zhao::check(accepted == kDepth,
                "fragments are accepted with the TMU refusing (ready is local)",
                kDepth, accepted);
    top.eval();
    zhao::check(top.f_ready_o == 0, "and the block goes full at DEPTH, not before",
                0, top.f_ready_o);
    zhao::check(top.full_clocks_o > 0, "full clocks are counted once storage runs out",
                1, top.full_clocks_o > 0 ? 1 : 0);
  }

  // --------------------------------------------------------------- 2 ---
  // A 3-SAMPLE FRAGMENT DOES NOT RETIRE EARLY.
  {
    reset(top);
    Frag f{3, {11, 22, 33}, {44, 55, 66}, 0, 0xBEEF, false};
    zhao::check(offer(top, f), "the 3-sample fragment is accepted", 1, 1);

    // Let it issue all three requests.
    int issued = 0;
    for (int c = 0; c < 32 && issued < 3; ++c) {
      top.eval();
      if (top.tmu_valid_o) ++issued;
      zhao::tick(top);
    }
    zhao::check(issued == 3, "all three samples are issued to the single TMU", 3, issued);

    // Return only samples 0 and 1.
    for (int s = 0; s < 2; ++s) {
      top.tmu_rvalid_i = 1;
      top.tmu_rslot_i = 0;
      top.tmu_rsidx_i = s;
      top.tmu_rgen_i = 1;
      top.tmu_rgb_i = 0x101010u * (s + 1);
      top.tmu_a_i = 0xF0;
      zhao::tick(top);
      top.tmu_rvalid_i = 0;
    }
    top.eval();
    zhao::check(top.o_valid_o == 0, "it does NOT retire on two of three samples",
                0, top.o_valid_o);

    // The third completes it.
    top.tmu_rvalid_i = 1;
    top.tmu_rslot_i = 0;
    top.tmu_rsidx_i = 2;
    top.tmu_rgen_i = 1;
    top.tmu_rgb_i = 0x303030u;
    top.tmu_a_i = 0xF0;
    zhao::tick(top);
    top.tmu_rvalid_i = 0;
    top.eval();
    zhao::check(top.o_valid_o == 1, "and it retires once the third arrives",
                1, top.o_valid_o);
  }

  // --------------------------------------------------------------- 3 ---
  // COMPLETION ORDER IS NOT RETIREMENT ORDER.
  {
    reset(top);
    constexpr int kN = 4;
    for (int i = 0; i < kN; ++i) {
      Frag f{1, {static_cast<uint32_t>(i), 0, 0}, {0, 0, 0}, 0,
             0xC000u + static_cast<uint64_t>(i), false};
      zhao::check(offer(top, f), "batch fragment accepted", 1, 1);
    }
    // Drain the issue side so every slot has a request outstanding.
    for (int c = 0; c < 32; ++c) { top.eval(); zhao::tick(top); }

    // Return them BACKWARDS.
    for (int i = kN - 1; i >= 0; --i) {
      top.tmu_rvalid_i = 1;
      top.tmu_rslot_i = i;
      top.tmu_rsidx_i = 0;
      top.tmu_rgen_i = 1;
      top.tmu_rgb_i = 0x010000u * static_cast<uint32_t>(i + 1);
      top.tmu_a_i = 0x80;
      zhao::tick(top);
      top.tmu_rvalid_i = 0;
    }

    std::vector<uint64_t> got;
    top.o_ready_i = 1;
    for (int c = 0; c < 64 && static_cast<int>(got.size()) < kN; ++c) {
      top.eval();
      if (top.o_valid_o) got.push_back(top.o_ctx_o);
      zhao::tick(top);
    }
    bool in_order = got.size() == kN;
    for (size_t i = 0; i < got.size(); ++i)
      if (got[i] != 0xC000u + i) in_order = false;
    zhao::check(in_order,
                "returns arriving backwards still retire in ALLOCATION order",
                1, in_order ? 1 : 0);
  }

  // --------------------------------------------------------------- 4 ---
  // A STALE GENERATION IS REJECTED, NOT WRITTEN INTO THE NEW OCCUPANT.
  {
    reset(top);
    Frag f{1, {7, 0, 0}, {8, 0, 0}, 0, 0xD001, false};
    offer(top, f);
    for (int c = 0; c < 8; ++c) { top.eval(); zhao::tick(top); }

    const uint32_t before = top.id_errors_o;
    // Generation 2 when the live entry is generation 1: a response from a
    // previous occupant of this slot.
    top.tmu_rvalid_i = 1;
    top.tmu_rslot_i = 0;
    top.tmu_rsidx_i = 0;
    top.tmu_rgen_i = 2;
    top.tmu_rgb_i = 0xDEADBEu;
    top.tmu_a_i = 0xFF;
    zhao::tick(top);
    top.tmu_rvalid_i = 0;
    top.eval();

    zhao::check(top.id_errors_o == before + 1,
                "a stale-generation return is counted as an identity error",
                static_cast<int>(before + 1), static_cast<int>(top.id_errors_o));
    zhao::check(top.id_error_o == 1, "and the sticky flag latches", 1, top.id_error_o);
    zhao::check(top.o_valid_o == 0,
                "and it does NOT complete the live fragment it collided with",
                0, top.o_valid_o);
  }

  // --------------------------------------------------------------- 5 ---
  // THE COMBINER IS UNFROZEN AND SAYS SO.
  {
    reset(top);
    Frag f{1, {1, 0, 0}, {2, 0, 0}, /*recipe=*/1 /* MODULATE */, 0xE001, false};
    offer(top, f);
    for (int c = 0; c < 8; ++c) { top.eval(); zhao::tick(top); }
    top.tmu_rvalid_i = 1;
    top.tmu_rslot_i = 0;
    top.tmu_rsidx_i = 0;
    top.tmu_rgen_i = 1;
    top.tmu_rgb_i = 0x123456u;
    top.tmu_a_i = 0x77;
    zhao::tick(top);
    top.tmu_rvalid_i = 0;
    top.eval();
    zhao::check(top.o_valid_o == 1, "a MODULATE fragment still retires", 1, top.o_valid_o);
    zhao::check(top.combiner_unfrozen_o == 1,
                "but combiner_unfrozen_o is RAISED -- the blend is not frozen yet",
                1, top.combiner_unfrozen_o);
    zhao::check(top.o_rgb_o == 0x123456u,
                "and it returns sample 0 rather than invented arithmetic",
                0x123456, static_cast<int>(top.o_rgb_o));
  }

  // --------------------------------------------------------------- 6 ---
  // PASSTHROUGH IS EXACT AND DOES NOT RAISE THE FLAG.
  {
    reset(top);
    Frag f{1, {1, 0, 0}, {2, 0, 0}, /*recipe=*/0 /* PASSTHRU */, 0xF001, false};
    offer(top, f);
    for (int c = 0; c < 8; ++c) { top.eval(); zhao::tick(top); }
    top.tmu_rvalid_i = 1;
    top.tmu_rslot_i = 0;
    top.tmu_rsidx_i = 0;
    top.tmu_rgen_i = 1;
    top.tmu_rgb_i = 0xABCDEFu;
    top.tmu_a_i = 0x42;
    zhao::tick(top);
    top.tmu_rvalid_i = 0;
    top.eval();
    zhao::check(top.combiner_unfrozen_o == 0,
                "PASSTHRU does not raise the unfrozen flag -- its result is exact",
                0, top.combiner_unfrozen_o);
    zhao::check(top.o_rgb_o == 0xABCDEFu && top.o_a_o == 0x42,
                "and passes the sample through unchanged", 1,
                (top.o_rgb_o == 0xABCDEFu && top.o_a_o == 0x42) ? 1 : 0);
  }

  return zhao::report_and_exit("raster_texjoin_v2_directed");
}
