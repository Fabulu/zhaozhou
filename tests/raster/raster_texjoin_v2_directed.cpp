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
#include <map>
#include <vector>

#include "verilated.h"

#include "Vzhao_raster_texjoin_v2.h"

#include "zhao_sim.hpp"

namespace {

constexpr int kDepth = 16;

struct Frag {
  int count;  // sample_count 0..3
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
    top.tmu_ready_i = 0;  // the TMU never accepts anything
    top.o_ready_i = 1;
    int accepted = 0;
    for (int i = 0; i < kDepth + 4; ++i) {
      Frag f{1, {100u + i, 0, 0}, {200u + i, 0, 0}, 0, 0xA000u + i, false};
      if (offer(top, f)) ++accepted;
    }
    zhao::check(accepted == kDepth, "fragments are accepted with the TMU refusing (ready is local)",
                kDepth, accepted);
    top.eval();
    zhao::check(top.f_ready_o == 0, "and the block goes full at DEPTH, not before", 0,
                top.f_ready_o);
    zhao::check(top.full_clocks_o > 0, "full clocks are counted once storage runs out", 1,
                top.full_clocks_o > 0 ? 1 : 0);
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
    zhao::check(top.o_valid_o == 0, "it does NOT retire on two of three samples", 0, top.o_valid_o);

    // The third completes it.
    top.tmu_rvalid_i = 1;
    top.tmu_rslot_i = 0;
    top.tmu_rsidx_i = 2;
    top.tmu_rgen_i = 1;
    top.tmu_rgb_i = 0x303030u;
    top.tmu_a_i = 0xF0;
    zhao::tick(top);
    top.tmu_rvalid_i = 0;
    // The retirement packet is REGISTERED (X3.7), so completion and the
    // output are one clock apart. That is the point of registering it: the
    // packet cannot change under a stalled consumer.
    zhao::tick(top);
    top.eval();
    zhao::check(top.o_valid_o == 1, "and it retires once the third arrives", 1, top.o_valid_o);
  }

  // --------------------------------------------------------------- 3 ---
  // COMPLETION ORDER IS NOT RETIREMENT ORDER.
  {
    reset(top);
    constexpr int kN = 4;
    for (int i = 0; i < kN; ++i) {
      Frag f{1, {static_cast<uint32_t>(i), 0, 0},   {0, 0, 0},
             0, 0xC000u + static_cast<uint64_t>(i), false};
      zhao::check(offer(top, f), "batch fragment accepted", 1, 1);
    }
    // Drain the issue side so every slot has a request outstanding.
    for (int c = 0; c < 32; ++c) {
      top.eval();
      zhao::tick(top);
    }

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
    zhao::check(in_order, "returns arriving backwards still retire in ALLOCATION order", 1,
                in_order ? 1 : 0);
  }

  // --------------------------------------------------------------- 4 ---
  // A STALE GENERATION IS REJECTED, NOT WRITTEN INTO THE NEW OCCUPANT.
  {
    reset(top);
    Frag f{1, {7, 0, 0}, {8, 0, 0}, 0, 0xD001, false};
    offer(top, f);
    for (int c = 0; c < 8; ++c) {
      top.eval();
      zhao::tick(top);
    }

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
    zhao::check(top.o_valid_o == 0, "and it does NOT complete the live fragment it collided with",
                0, top.o_valid_o);
  }

  // --------------------------------------------------------------- 5 ---
  // THE COMBINER IS UNFROZEN AND SAYS SO.
  {
    reset(top);
    Frag f{1, {1, 0, 0}, {2, 0, 0}, /*recipe=*/1 /* MODULATE */, 0xE001, false};
    offer(top, f);
    for (int c = 0; c < 8; ++c) {
      top.eval();
      zhao::tick(top);
    }
    top.tmu_rvalid_i = 1;
    top.tmu_rslot_i = 0;
    top.tmu_rsidx_i = 0;
    top.tmu_rgen_i = 1;
    top.tmu_rgb_i = 0x123456u;
    top.tmu_a_i = 0x77;
    zhao::tick(top);
    top.tmu_rvalid_i = 0;
    zhao::tick(top);
    top.eval();
    zhao::check(top.o_valid_o == 1, "a MODULATE fragment still retires", 1, top.o_valid_o);
    zhao::check(top.combiner_unfrozen_o == 1,
                "but combiner_unfrozen_o is RAISED -- the blend is not frozen yet", 1,
                top.combiner_unfrozen_o);
    zhao::check(top.o_rgb_o == 0x123456u, "and it returns sample 0 rather than invented arithmetic",
                0x123456, static_cast<int>(top.o_rgb_o));
  }

  // --------------------------------------------------------------- 6 ---
  // PASSTHROUGH IS EXACT AND DOES NOT RAISE THE FLAG.
  {
    reset(top);
    Frag f{1, {1, 0, 0}, {2, 0, 0}, /*recipe=*/0 /* PASSTHRU */, 0xF001, false};
    offer(top, f);
    for (int c = 0; c < 8; ++c) {
      top.eval();
      zhao::tick(top);
    }
    top.tmu_rvalid_i = 1;
    top.tmu_rslot_i = 0;
    top.tmu_rsidx_i = 0;
    top.tmu_rgen_i = 1;
    top.tmu_rgb_i = 0xABCDEFu;
    top.tmu_a_i = 0x42;
    zhao::tick(top);
    top.tmu_rvalid_i = 0;
    zhao::tick(top);
    top.eval();
    zhao::check(top.combiner_unfrozen_o == 0,
                "PASSTHRU does not raise the unfrozen flag -- its result is exact", 0,
                top.combiner_unfrozen_o);
    zhao::check(top.o_rgb_o == 0xABCDEFu && top.o_a_o == 0x42,
                "and passes the sample through unchanged", 1,
                (top.o_rgb_o == 0xABCDEFu && top.o_a_o == 0x42) ? 1 : 0);
  }

  // --------------------------------------------------------------- 7 ---
  // STEADY STATE: ACCEPT AND RETIRE ON THE SAME CLOCK, FOR A LONG TIME.
  //
  // This is the case the first six miss, and it is the one the owner's review
  // named. Cases 1-6 accept a batch, then drain it. A full elastic queue does
  // BOTH ON EVERY CLOCK, and the free counter was moved by two separate
  // nonblocking assignments in one always_ff -- so on such a cycle only the
  // last one landed and the count drifted UPWARD by one each time.
  //
  // The drift is invisible for a while. f_ready_o stays high past DEPTH, the
  // block admits a fragment into a slot that is still live, and the symptom is
  // an out-of-order or duplicated ctx hundreds of clocks after the fault. So
  // the check is not "does it look right" -- it is: outstanding never exceeds
  // DEPTH, every ctx comes out exactly once, in allocation order.
  //
  // Reverting the counter fix must make this FAIL. It does.
  {
    reset(top);
    struct Ret {
      int slot, sidx, gen, due;
    };
    std::deque<Ret> rets;
    std::deque<uint64_t> expect;
    uint64_t next_ctx = 0x9000;
    int accepted = 0, retired = 0, outstanding = 0, worst_outstanding = 0;
    int order_errors = 0, both_clocks = 0;
    uint32_t rs = 0xC0FFEEu;
    auto rnd = [&]() {
      rs = rs * 1664525u + 1013904223u;
      return rs >> 9;
    };

    for (int c = 0; c < 4000; ++c) {
      // offer a 1-sample PASSTHRU fragment every cycle
      top.f_valid_i = 1;
      top.f_sample_count_i = 1;
      for (int j = 0; j < 3; ++j) {
        top.f_u_i[j] = 0;
        top.f_v_i[j] = 0;
        top.f_binding_i[j] = 1;
        top.f_lod_i[j] = 0;
      }
      top.f_recipe_i = 0;
      top.f_ctx_i = next_ctx;
      top.f_aux_i = 0;
      top.f_uv_sat_i = 0;
      top.tmu_ready_i = 1;
      top.aux_ready_i = 1;
      top.aux_rvalid_i = 0;
      // stall the output sometimes, so the queue actually fills rather than
      // running empty -- an empty queue never does both on one clock
      top.o_ready_i = (rnd() % 4u) != 0u;

      // a TMU return that came due
      top.tmu_rvalid_i = 0;
      if (!rets.empty() && rets.front().due <= c) {
        const Ret r = rets.front();
        top.tmu_rvalid_i = 1;
        top.tmu_rslot_i = r.slot;
        top.tmu_rsidx_i = r.sidx;
        top.tmu_rgen_i = r.gen;
        top.tmu_rgb_i = 0x101010u;
        top.tmu_a_i = 0xFF;
        rets.pop_front();
      }

      top.eval();
      const bool acc = top.f_ready_o != 0;
      const bool ret = top.o_valid_o != 0 && top.o_ready_i != 0;
      if (acc && ret) ++both_clocks;
      if (top.tmu_valid_o && top.tmu_ready_i)
        rets.push_back({static_cast<int>(top.tmu_slot_o), static_cast<int>(top.tmu_sidx_o),
                        static_cast<int>(top.tmu_gen_o), c + 1 + static_cast<int>(rnd() % 12u)});
      if (ret) {
        if (expect.empty() || expect.front() != top.o_ctx_o)
          ++order_errors;
        else
          expect.pop_front();
        ++retired;
        --outstanding;
      }
      if (acc) {
        expect.push_back(next_ctx);
        ++next_ctx;
        ++accepted;
        ++outstanding;
        if (outstanding > worst_outstanding) worst_outstanding = outstanding;
      }
      zhao::tick(top);
    }

    zhao::check(both_clocks > 200, "the soak really did accept and retire on the same clock, often",
                1, both_clocks > 200 ? 1 : 0);
    // DEPTH + 1, and the +1 is a real storage location rather than slack in
    // the check: the retirement packet is now a REGISTER, so a fragment whose
    // slot has been freed is still inside the block until the consumer takes
    // it. The bound moved by exactly one because exactly one register was
    // added.
    //
    // This is the check that caught the free-count race, and it must stay
    // sharp. It is equality-tight, not relaxed: re-armed against the
    // restructured block on 2026-09-03, reverting the single-assignment fix
    // drives this to 1532 outstanding with 1686 fragments out of order and
    // 253 returns landing on recycled slots.
    zhao::check(worst_outstanding <= kDepth + 1,
                "outstanding fragments NEVER exceed DEPTH + the one registered "
                "output -- the free count does not drift when accept and retire "
                "coincide",
                kDepth + 1, worst_outstanding);
    zhao::check(order_errors == 0, "and every fragment retires exactly once, in allocation order",
                0, order_errors);
    zhao::check(top.id_errors_o == 0, "with no return ever landing on a recycled slot", 0,
                static_cast<int>(top.id_errors_o));
    std::printf("  soak: %d accepted, %d retired, %d same-clock, worst outstanding %d\n", accepted,
                retired, both_clocks, worst_outstanding);
  }

  // --------------------------------------------------------------- 8 ---
  // THE ZERO-SAMPLE PATH READS NO TEXEL, AND SAYS SO WITH A DEFINED VALUE.
  //
  // Ruled defect X3.6: `sample_count == 0` completed immediately and the
  // combiner still read sample-0 storage -- which holds whatever the previous
  // occupant of that slot left there. R9 says count 0 means has_texture = 0
  // and NO sample is read, so there is nothing legitimate for it to return.
  //
  // THE FIRST VERSION OF THIS CASE DID NOT TEST THAT, and passed against the
  // unfixed RTL. It dirtied slot 0, allocated DEPTH more fragments, and then
  // checked the LAST value on the output -- which came from a slot that had
  // never been dirtied. Removing the fix changed nothing and the case still
  // went green.
  //
  // What it has to do instead is follow the specific fragment that lands back
  // on the dirtied slot. Every retirement is captured by ctx, so the check is
  // about one identified fragment rather than about whatever happened to be
  // on the port at the end.
  {
    reset(top);
    std::map<uint64_t, uint32_t> retired_rgb;
    auto pump = [&](int clocks) {
      for (int c = 0; c < clocks; ++c) {
        top.eval();
        if (top.o_valid_o && top.o_ready_i) retired_rgb[top.o_ctx_o] = top.o_rgb_o;
        zhao::tick(top);
      }
    };

    // 1: dirty slot 0 with a recognisable texel.
    Frag one{1, {5, 0, 0}, {6, 0, 0}, /*recipe=*/0, 0xD001, false};
    offer(top, one);
    pump(4);
    top.tmu_rvalid_i = 1;
    top.tmu_rslot_i = 0;
    top.tmu_rsidx_i = 0;
    top.tmu_rgen_i = 1;
    top.tmu_rgb_i = 0xBADBADu;  // the value that must not reappear
    top.tmu_a_i = 0xEE;
    zhao::tick(top);
    top.tmu_rvalid_i = 0;
    pump(3);
    zhao::check(retired_rgb.count(0xD001) == 1 && retired_rgb[0xD001] == 0xBADBADu,
                "the slot really is dirtied first", 1,
                (retired_rgb.count(0xD001) == 1 && retired_rgb[0xD001] == 0xBADBADu) ? 1 : 0);

    // 2: walk zero-sample fragments all the way round the ring so that one of
    //    them lands back on slot 0. Their ctx values say which is which.
    for (int i = 0; i < kDepth + 2; ++i) {
      Frag z{0, {0, 0, 0}, {0, 0, 0}, 0, 0xD100u + static_cast<uint64_t>(i), false};
      offer(top, z);
      pump(3);
    }
    pump(8);

    // 3: EVERY one of them must have read no texel. Exactly one of them
    //    occupied slot 0, and without the fix it is the one that returns
    //    0xBADBAD -- but the case does not need to know which.
    int leaked = 0, seen = 0;
    for (int i = 0; i < kDepth + 2; ++i) {
      const uint64_t ctx = 0xD100u + static_cast<uint64_t>(i);
      if (retired_rgb.count(ctx) == 0) continue;
      ++seen;
      if (retired_rgb[ctx] == 0xBADBADu) ++leaked;
    }
    zhao::check(seen >= kDepth,
                "the zero-sample fragments really did go all the way round the "
                "ring, so one of them reused the dirtied slot",
                1, seen >= kDepth ? 1 : 0);
    zhao::check(leaked == 0,
                "and NOT ONE of them returns the previous occupant's texel -- a "
                "ZERO-sample fragment read no texel at all",
                0, leaked);
  }

  // --------------------------------------------------------------- 9 ---
  // THE OUTPUT IS HELD UNDER A STALL.
  //
  // This is the property a registered retirement packet exists for, and the
  // one a combinational view of table storage cannot have: with the consumer
  // stalled, the packet on the output must not change, even while the block
  // keeps accepting and completing other fragments behind it.
  {
    reset(top);
    Frag f{1, {9, 0, 0}, {9, 0, 0}, /*recipe=*/0, 0xC0DEu, false};
    offer(top, f);
    for (int c = 0; c < 4; ++c) {
      top.eval();
      zhao::tick(top);
    }
    top.tmu_rvalid_i = 1;
    top.tmu_rslot_i = 0;
    top.tmu_rsidx_i = 0;
    top.tmu_rgen_i = 1;
    top.tmu_rgb_i = 0xFACADEu;
    top.tmu_a_i = 0x5A;
    zhao::tick(top);
    top.tmu_rvalid_i = 0;
    zhao::tick(top);

    top.o_ready_i = 0;  // STALL the consumer
    top.eval();
    const uint32_t held_rgb = top.o_rgb_o;
    const uint32_t held_a = top.o_a_o;
    const uint64_t held_ctx = top.o_ctx_o;
    zhao::check(top.o_valid_o == 1 && held_rgb == 0xFACADEu, "the packet is presented", 1,
                (top.o_valid_o && held_rgb == 0xFACADEu) ? 1 : 0);

    // keep the queue busy behind it for a long stall
    int changed = 0;
    for (int c = 0; c < 40; ++c) {
      Frag g{1, {1, 0, 0}, {1, 0, 0}, 0, 0xE000u + static_cast<uint64_t>(c), false};
      top.f_valid_i = 1;
      top.f_sample_count_i = g.count;
      for (int j = 0; j < 3; ++j) {
        top.f_u_i[j] = 1;
        top.f_v_i[j] = 1;
        top.f_binding_i[j] = 1;
        top.f_lod_i[j] = 0;
      }
      top.f_recipe_i = 0;
      top.f_ctx_i = g.ctx;
      top.f_aux_i = 0;
      top.f_uv_sat_i = 0;
      if (top.tmu_valid_o) {
        top.tmu_rvalid_i = 1;
        top.tmu_rslot_i = top.tmu_slot_o;
        top.tmu_rsidx_i = top.tmu_sidx_o;
        top.tmu_rgen_i = top.tmu_gen_o;
        top.tmu_rgb_i = 0x111111u;
        top.tmu_a_i = 0x22;
      } else {
        top.tmu_rvalid_i = 0;
      }
      top.eval();
      if (top.o_valid_o != 1 || top.o_rgb_o != held_rgb || top.o_a_o != held_a ||
          top.o_ctx_o != held_ctx)
        ++changed;
      zhao::tick(top);
    }
    top.f_valid_i = 0;
    top.tmu_rvalid_i = 0;
    zhao::check(changed == 0,
                "and it does NOT change for 40 clocks of stall while the block "
                "keeps working behind it",
                0, changed);
    top.o_ready_i = 1;
  }

  // -------------------------------------------------------------- 10 ---
  // A DUPLICATE RETURN IS COUNTED, NOT APPLIED TWICE.
  //
  // Ruled defect X3.5. The second copy carries the same slot and generation as
  // the first, so identity alone cannot reject it -- what makes it harmless is
  // that a return WRITES storage rather than incrementing anything, and that
  // completion is `arr == req` rather than a count.
  {
    reset(top);
    Frag f{2, {3, 4, 0}, {5, 6, 0}, /*recipe=*/0, 0xD0FFEE, false};
    offer(top, f);
    for (int c = 0; c < 4; ++c) {
      top.eval();
      zhao::tick(top);
    }

    // sample 0 arrives TWICE
    for (int rep = 0; rep < 2; ++rep) {
      top.tmu_rvalid_i = 1;
      top.tmu_rslot_i = 0;
      top.tmu_rsidx_i = 0;
      top.tmu_rgen_i = 1;
      top.tmu_rgb_i = 0x010203u;
      top.tmu_a_i = 0x44;
      zhao::tick(top);
      top.tmu_rvalid_i = 0;
      zhao::tick(top);
    }
    top.eval();
    zhao::check(top.o_valid_o == 0,
                "a DUPLICATE sample-0 return does not complete a 2-sample "
                "fragment -- completion is a mask, not a count",
                0, top.o_valid_o);
    zhao::check(top.id_errors_o == 0,
                "and it is not an identity error either: the slot and "
                "generation were both correct",
                0, static_cast<int>(top.id_errors_o));

    // the real sample 1 still completes it
    top.tmu_rvalid_i = 1;
    top.tmu_rslot_i = 0;
    top.tmu_rsidx_i = 1;
    top.tmu_rgen_i = 1;
    top.tmu_rgb_i = 0x0A0B0Cu;
    top.tmu_a_i = 0x55;
    zhao::tick(top);
    top.tmu_rvalid_i = 0;
    zhao::tick(top);
    top.eval();
    zhao::check(top.o_valid_o == 1, "and the missing sample still completes it", 1, top.o_valid_o);
  }

  // -------------------------------------------------------------- 11 ---
  // THE WORK QUEUE NEVER OVERFLOWS.
  {
    zhao::check(top.wq_overflow_o == 0,
                "the sample work queue never overflowed across every case above "
                "-- the by-construction bound, checked rather than asserted",
                0, top.wq_overflow_o);
  }

  return zhao::report_and_exit("raster_texjoin_v2_directed");
}
