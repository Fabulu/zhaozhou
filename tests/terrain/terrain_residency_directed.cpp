// terrain_residency_directed.cpp — can the island directory ever hand a patch
// job somebody else's ground?
//
// ---------------------------------------------------------------------------
// THE FAILURES THIS FILE EXISTS FOR
// ---------------------------------------------------------------------------
// A residency directory has two ways to be catastrophically wrong, and both
// look fine in a static frame:
//
//   * it reports a page RESIDENT before the loader has filled it, so a patch
//     job composes an unwritten height lattice;
//   * it lets an in-flight job keep a handle to a slot that has since been
//     re-claimed, so the job reads ANOTHER PATCH'S GROUND -- terrain from one
//     island appearing inside another.
//
// Neither reproduces at rest. Both need residency to change WHILE work is
// outstanding, which is exactly what prefetch does during fast traversal, and
// is why `reports/Missingterrain` asks for the validation to be an 8 km
// traversal capture rather than a static image.
//
// THE SAME-CYCLE fin+claim CASE IS THE ONE THAT WAS ACTUALLY BROKEN. The first
// version of the RTL guarded the loader's completion by generation alone. But
// `gen_r` is a register, so during the cycle a claim lands it still holds the
// OLD generation -- a stale `fin` matched, and being later in the same
// always_ff its `load_r <= 1` overwrote the claim's `load_r <= 0`. A fresh
// unfilled page came out marked LOADED. Found by inspection because there was
// no test; this is that test.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_residency.h"

#include "zhao_sim.hpp"

namespace {

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_residency top;

  auto idle = [&]() {
    top.lu_valid_i = 0;
    top.cl_valid_i = 0;
    top.fin_valid_i = 0;
    top.dirty_valid_i = 0;
    top.chk_valid_i = 0;
  };
  auto reset = [&]() {
    idle();
    top.rst_n = 0;
    for (int i = 0; i < 6; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  // claim a patch, returning its slot and generation
  auto claim = [&](int px, int py, uint32_t* slot, uint32_t* gen) {
    idle();
    top.cl_valid_i = 1;
    top.cl_px_i = px;
    top.cl_py_i = py;
    zhao::tick(top);
    idle();
    top.eval();
    *slot = top.cl_slot_o;
    *gen = top.cl_gen_o;
  };
  auto finish = [&](uint32_t slot, uint32_t gen) {
    idle();
    top.fin_valid_i = 1;
    top.fin_slot_i = slot;
    top.fin_gen_i = gen;
    zhao::tick(top);
    idle();
  };
  auto lookup = [&](int px, int py, int* hit) {
    idle();
    top.lu_valid_i = 1;
    top.lu_px_i = px;
    top.lu_py_i = py;
    zhao::tick(top);
    idle();
    top.eval();
    *hit = top.lu_hit_o;
  };

  // ---- 1: a claimed-but-unloaded page is NOT resident ---------------------
  {
    reset();
    uint32_t slot, gen;
    claim(10, 20, &slot, &gen);
    int hit = 1;
    lookup(10, 20, &hit);
    zhao::check(hit == 0, "a claimed page is NOT resident until the loader finishes it", 0, hit);
    finish(slot, gen);
    lookup(10, 20, &hit);
    zhao::check(hit == 1, "and IS resident once it does", 1, hit);
  }

  // ---- 2: a different patch in the same slot does not answer -------------
  // Direct-mapped on the low 5 bits of each axis, so (10,20) and (42,52)
  // collide. The tag must still reject the wrong one.
  {
    reset();
    uint32_t slot, gen;
    claim(10, 20, &slot, &gen);
    finish(slot, gen);
    int hit = 1;
    lookup(42, 52, &hit);  // same slot, different patch
    zhao::check(hit == 0, "a COLLIDING patch does not answer with the resident one's page", 0, hit);
  }

  // ---- 3: THE HAZARD -- fin and claim on the same slot, same clock --------
  // The stale fin must not mark the fresh page loaded.
  {
    reset();
    uint32_t slot, gen;
    claim(3, 4, &slot, &gen);
    finish(slot, gen);

    // Now re-claim the SAME SLOT for a different patch while the OLD load
    // completes on that very clock.
    idle();
    top.cl_valid_i = 1;
    top.cl_px_i = 3 + 32;  // collides with (3,4)
    top.cl_py_i = 4 + 32;
    top.fin_valid_i = 1;  // the previous page's loader, arriving late
    top.fin_slot_i = slot;
    top.fin_gen_i = gen;  // the OLD generation
    zhao::tick(top);
    idle();
    top.eval();

    int hit = 1;
    lookup(3 + 32, 4 + 32, &hit);
    zhao::check(hit == 0,
                "a late fin on a JUST-RECLAIMED slot does NOT mark the fresh "
                "page loaded",
                0, hit);
  }

  // ---- 4: stale handles are reported, never silently redirected ----------
  {
    reset();
    uint32_t slot, gen;
    claim(7, 8, &slot, &gen);
    finish(slot, gen);

    // an in-flight job holds {slot, gen}; the slot is re-claimed underneath it
    uint32_t slot2, gen2;
    claim(7 + 32, 8 + 32, &slot2, &gen2);
    zhao::check(slot2 == slot, "the colliding patch takes the same slot", slot, slot2);

    idle();
    top.chk_valid_i = 1;
    top.chk_slot_i = slot;
    top.chk_gen_i = gen;  // the OLD handle
    zhao::tick(top);
    idle();
    top.eval();
    zhao::check(top.chk_stale_o == 1, "an in-flight handle to a re-claimed slot is reported STALE",
                1, top.chk_stale_o);

    // and the new handle is not
    idle();
    top.chk_valid_i = 1;
    top.chk_slot_i = slot2;
    top.chk_gen_i = gen2;
    zhao::tick(top);
    idle();
    top.eval();
    zhao::check(top.chk_stale_o == 0, "while the current handle is not", 0, top.chk_stale_o);
  }

  // ---- 5: re-claiming the SAME patch must not invalidate live handles ----
  // A visible-set rebuild re-submits patches that are already resident. If that
  // advanced the generation, every in-flight job would be told it is stale on
  // every frame and the pipeline would never make progress.
  {
    reset();
    uint32_t slot, gen;
    claim(11, 12, &slot, &gen);
    finish(slot, gen);
    uint32_t slot2, gen2;
    claim(11, 12, &slot2, &gen2);  // the same patch again
    zhao::check(gen2 == gen, "re-claiming the SAME patch does not advance the generation", gen,
                gen2);
    zhao::check(top.cl_evicted_o == 0, "and reports no eviction", 0, top.cl_evicted_o);
    int hit = 0;
    lookup(11, 12, &hit);
    zhao::check(hit == 1, "and the page stays resident", 1, hit);
  }

  // ---- 6: a dirty eviction is reported so scars can be written back ------
  {
    reset();
    uint32_t slot, gen;
    claim(5, 6, &slot, &gen);
    finish(slot, gen);
    idle();
    top.dirty_valid_i = 1;
    top.dirty_slot_i = slot;
    top.dirty_gen_i = gen;
    zhao::tick(top);
    idle();

    uint32_t s2, g2;
    claim(5 + 32, 6 + 32, &s2, &g2);
    zhao::check(top.cl_evicted_o == 1, "displacing a live page reports an eviction", 1,
                top.cl_evicted_o);
    zhao::check(top.cl_evicted_dirty_o == 1,
                "and flags it DIRTY so its scars are written back before reuse", 1,
                top.cl_evicted_dirty_o);
    zhao::check(top.cl_evicted_px_o == 5 && top.cl_evicted_py_o == 6,
                "and names the patch that was displaced", 1,
                (top.cl_evicted_px_o == 5 && top.cl_evicted_py_o == 6) ? 1 : 0);
  }

  // ---- 7: a traversal sweep -- residency stays consistent ----------------
  // Walk a camera across patches and keep claiming/finishing. Every patch that
  // was finished and not since displaced must still read back resident.
  {
    reset();
    uint32_t s = 0x1234u;
    int bad = 0;
    for (int i = 0; i < 300; ++i) {
      const int px = static_cast<int>(rnd(&s) % 20);
      const int py = static_cast<int>(rnd(&s) % 20);
      uint32_t slot, gen;
      claim(px, py, &slot, &gen);
      finish(slot, gen);
      int hit = 0;
      lookup(px, py, &hit);
      if (!hit) ++bad;  // just claimed and finished: must be resident
    }
    zhao::check(bad == 0, "across a 300-patch sweep, every just-loaded patch reads resident", 0,
                bad);
    std::printf("  sweep: %u hits, %u misses, %u evictions, %u collisions, %u resident\n",
                top.hits_o, top.misses_o, top.evictions_o, top.collisions_o, top.resident_o);
  }

  return zhao::report_and_exit("terrain_residency_directed");
}
