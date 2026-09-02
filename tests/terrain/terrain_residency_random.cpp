// terrain_residency_random.cpp — a randomized traversal against an independent
// model of what the directory is supposed to say.
//
// ---------------------------------------------------------------------------
// WHY RANDOM, WHEN THE DIRECTED SUITE ALREADY PASSES
// ---------------------------------------------------------------------------
// The directed suite tests the cases I thought of. The failure that actually
// matters here is one nobody thinks of: `reports/Missingterrain` asks for the
// validation to be an 8 km TRAVERSAL — "fly rapidly across the world, force
// residency churn, deform patches, leave them, return later, and verify that
// geometry, scars, breaches, LOD seams and both split-screen views remain
// exact."
//
// That is a sequence property, not a state property. It needs a camera that
// keeps moving while loads land late, pages go dirty, and slots are re-taken
// underneath in-flight work.
//
// ---------------------------------------------------------------------------
// THE MODEL IS INDEPENDENT, AND ONE PROPERTY MATTERS MOST
// ---------------------------------------------------------------------------
// The C++ model is written from the CONTRACT — resident means claimed AND
// loaded AND not since displaced — not from the RTL's structure. If it were a
// transcription of the RTL it would agree with it about the same mistakes.
//
// The property worth the file is DIRTY EVICTION. A dirty page holds permanent
// scars and breaches: ground the player destroyed. If the directory ever
// displaces a dirty page WITHOUT reporting it, that terrain silently heals —
// the player returns to a crater they made and finds it gone. No frame is
// wrong, no test of a single frame can see it, and it is unrecoverable because
// the data is already overwritten.
//
// So the model tracks dirtiness independently and every eviction is compared.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_residency.h"

#include "zhao_sim.hpp"

namespace {

constexpr int SLOTS = 1024;

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

// The directory as the CONTRACT describes it, not as the RTL implements it.
struct Model {
  struct Slot {
    bool present = false;
    bool loaded = false;
    bool dirty = false;
    int px = 0, py = 0;
    uint32_t gen = 0;
  };
  Slot s[SLOTS];

  static int slot_of(int px, int py) {
    return ((py & 31) << 5) | (px & 31);
  }

  bool resident(int px, int py) const {
    const Slot& e = s[slot_of(px, py)];
    return e.present && e.loaded && e.px == px && e.py == py;
  }
};

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
  idle();
  top.rst_n = 0;
  for (int i = 0; i < 6; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  Model m;
  uint32_t s = 0xA11CE5u;

  // A camera walking a path, with a working set around it. Patches are claimed
  // as they come into range and loads land a random number of clocks later --
  // which is what creates the churn the directed suite cannot produce.
  struct Pending { int px, py; uint32_t slot, gen; int due; };
  std::vector<Pending> pending;

  int cam_x = 0, cam_y = 0;
  int bad_resident = 0, bad_evict = 0, bad_dirty_evict = 0, bad_stale = 0;
  int evictions_seen = 0, dirty_evictions_seen = 0, claims = 0;

  for (int step = 0; step < 6000; ++step) {
    idle();

    // ---- the camera TRAVERSES, it does not wander -------------------------
    // The first version drifted inside a 60x60 box and produced ZERO evictions,
    // because the direct map has a 2,048 m period: two patches collide only
    // when they differ by 32 in BOTH axes, which a small box almost never
    // reaches. The test exercised nothing it was written to exercise.
    //
    // A long walk is also what the brief actually asks for -- "fly rapidly
    // across the world, force residency churn ... leave them, return later".
    // The camera now crosses 200 patches (12.8 km at 64 m a patch) and turns
    // around, so slots are genuinely revisited by DIFFERENT patches.
    cam_x = (step / 12) % 200;
    cam_y = ((step / 40) % 70);
    if (((step / 2400) & 1) != 0) cam_x = 199 - cam_x;   // and back again

    // ---- one of: claim / finish / dirty / lookup / check ------------------
    const uint32_t act = rnd(&s) % 100u;

    if (act < 30) {
      // CLAIM a patch near the camera
      const int px = cam_x + static_cast<int>(rnd(&s) % 5u);
      const int py = cam_y + static_cast<int>(rnd(&s) % 5u);
      const int sl = Model::slot_of(px, py);
      Model::Slot& e = m.s[sl];
      const bool same = e.present && e.px == px && e.py == py;
      const bool will_evict = e.present && !same;
      const bool will_evict_dirty = will_evict && e.dirty;
      const int evict_px = e.px, evict_py = e.py;

      top.cl_valid_i = 1;
      top.cl_px_i = px;
      top.cl_py_i = py;
      zhao::tick(top);
      idle();
      top.eval();
      ++claims;

      if (top.cl_evicted_o != (will_evict ? 1 : 0)) ++bad_evict;
      if (top.cl_evicted_dirty_o != (will_evict_dirty ? 1 : 0)) ++bad_dirty_evict;
      if (will_evict &&
          (top.cl_evicted_px_o != evict_px || top.cl_evicted_py_o != evict_py))
        ++bad_evict;
      if (will_evict) ++evictions_seen;
      if (will_evict_dirty) ++dirty_evictions_seen;

      // update the model
      if (!same) {
        e.gen = (e.gen + 1u) & 0xFu;
        e.px = px;
        e.py = py;
        e.loaded = false;
        e.dirty = false;
        e.present = true;
        // a load will land later; anything already pending for this slot is
        // now stale and must never mark the new page loaded
        pending.push_back({px, py, static_cast<uint32_t>(sl), e.gen,
                           step + 1 + static_cast<int>(rnd(&s) % 20u)});
      }
    } else if (act < 45) {
      // FINISH the oldest due load, if any
      for (size_t i = 0; i < pending.size(); ++i) {
        if (pending[i].due <= step) {
          const Pending p = pending[i];
          pending.erase(pending.begin() + static_cast<long>(i));
          top.fin_valid_i = 1;
          top.fin_slot_i = p.slot;
          top.fin_gen_i = p.gen;
          zhao::tick(top);
          idle();
          Model::Slot& e = m.s[p.slot];
          // Only marks loaded if the slot still belongs to that generation.
          if (e.present && e.gen == p.gen) e.loaded = true;
          break;
        }
      }
    } else if (act < 55) {
      // DEFORM a resident patch -- this is what makes a page dirty
      const int px = cam_x + static_cast<int>(rnd(&s) % 5u);
      const int py = cam_y + static_cast<int>(rnd(&s) % 5u);
      const int sl = Model::slot_of(px, py);
      Model::Slot& e = m.s[sl];
      if (e.present && e.px == px && e.py == py) {
        top.dirty_valid_i = 1;
        top.dirty_slot_i = static_cast<uint32_t>(sl);
        top.dirty_gen_i = e.gen;
        zhao::tick(top);
        idle();
        e.dirty = true;
      }
    } else if (act < 85) {
      // LOOK UP a patch, near the camera or far away
      const int px = ((rnd(&s) & 1u) ? cam_x : 0) + static_cast<int>(rnd(&s) % 40u);
      const int py = ((rnd(&s) & 1u) ? cam_y : 0) + static_cast<int>(rnd(&s) % 40u);
      top.lu_valid_i = 1;
      top.lu_px_i = px;
      top.lu_py_i = py;
      zhao::tick(top);
      idle();
      top.eval();
      if (top.lu_hit_o != (m.resident(px, py) ? 1 : 0)) ++bad_resident;
    } else {
      // CHECK a handle, sometimes a deliberately stale one
      const int sl = static_cast<int>(rnd(&s) % SLOTS);
      const Model::Slot& e = m.s[sl];
      const uint32_t g = (rnd(&s) & 1u) ? e.gen : ((e.gen + 1u) & 0xFu);
      top.chk_valid_i = 1;
      top.chk_slot_i = static_cast<uint32_t>(sl);
      top.chk_gen_i = g;
      zhao::tick(top);
      idle();
      top.eval();
      const bool want_stale = !e.present || e.gen != g;
      if (top.chk_stale_o != (want_stale ? 1 : 0)) ++bad_stale;
    }
  }

  zhao::check(bad_resident == 0,
              "residency always matches the contract model across 4000 steps", 0,
              bad_resident);
  zhao::check(bad_stale == 0, "and every handle check agrees", 0, bad_stale);
  zhao::check(bad_evict == 0,
              "every eviction is reported, and names the displaced patch", 0,
              bad_evict);

  // THE ONE THAT MATTERS: a dirty page displaced without being reported means
  // permanent scars are silently lost.
  zhao::check(bad_dirty_evict == 0,
              "NO dirty page is ever displaced without being flagged for writeback",
              0, bad_dirty_evict);

  // and the traversal has to have actually exercised the interesting cases
  zhao::check(evictions_seen > 20, "the traversal forced real eviction churn", 1,
              evictions_seen > 20 ? 1 : 0);
  zhao::check(dirty_evictions_seen > 0,
              "including dirty pages being displaced", 1,
              dirty_evictions_seen > 0 ? 1 : 0);

  std::printf("  %d claims, %d evictions (%d dirty), hw: %u hits %u misses %u collisions\n",
              claims, evictions_seen, dirty_evictions_seen, top.hits_o, top.misses_o,
              top.collisions_o);

  return zhao::report_and_exit("terrain_residency_random");
}
