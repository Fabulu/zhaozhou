// part_ladder_directed.cpp — does the representation ladder pick the right
// rung, and does it stay picked?
//
// ---------------------------------------------------------------------------
// THE TWO HALVES OF THE RULING, AND THEY PULL AGAINST EACH OTHER
// ---------------------------------------------------------------------------
//   > A particle MAY change representation while it is alive.
//   > The simulation state NEVER changes because of the representation.
//
// The first half is why hysteresis exists: a particle hovering on a threshold
// would otherwise flicker between a triangle and a sprite every frame, and
// that flicker is the whole reason the hold count is in the record.
//
// The second half is why this block has no feedback path at all. It cannot
// reseed, respawn or perturb, and the check for that is structural rather than
// behavioural: there is no port with which to try. What CAN be tested is that
// the same particle decided twice for two cameras gets two answers and one
// simulation record — which is what "per camera and per frame" means.
//
// ---------------------------------------------------------------------------
// OVERLAPPING BANDS ARE THE INTERESTING PART
// ---------------------------------------------------------------------------
// Triangle is ~6-18 px and soft sprite is ~2-8, so a 7 px particle satisfies
// both. The ladder is an ORDER and the resolution is the first rung that fits,
// coarse to fine. A test that only used sizes inside one band would pass
// against a block that resolved the overlap the other way round.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_part_ladder.h"

#include "zhao_sim.hpp"
#include "zref/zref_particle.hpp"

namespace {

constexpr int MESHLET = 0, SHARD = 1, RIBBON = 2, SPRITE = 3, GLINT = 4,
              CULLED = 5;

constexpr int px(double v) { return static_cast<int>(v * 256.0); }

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_part_ladder top;

  auto reset = [&]() {
    top.v_valid_i = 0;
    top.r_ready_i = 1;
    top.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  struct Out { int rung, hold, changed; };

  auto decide = [&](int size, int trail, bool narrow, bool prot, int gov,
                    int prev, int hold, bool first) {
    top.v_valid_i = 1;
    top.p_size_i = size;
    top.p_trail_i = trail;
    top.p_narrow_i = narrow ? 1 : 0;
    top.p_protected_i = prot ? 1 : 0;
    top.p_gov_floor_i = gov;
    top.p_prev_rung_i = prev;
    top.p_hold_i = hold;
    top.p_first_i = first ? 1 : 0;
    top.eval();
    zhao::tick(top);
    top.v_valid_i = 0;
    top.eval();
    return Out{static_cast<int>(top.r_rung_o), static_cast<int>(top.r_hold_o),
               static_cast<int>(top.r_changed_o)};
  };

  reset();

  // ---- 1: each band picks its own rung on a first decision ---------------
  {
    struct C { double size; int want; const char* why; };
    const C cases[] = {
        {40.0, MESHLET, "far above the meshlet threshold"},
        {18.0, MESHLET, "exactly at it"},
        {17.9, SHARD,   "just below it"},
        {6.0,  SHARD,   "at the shard threshold"},
        {5.9,  SPRITE,  "below shard, above sprite"},
        {2.0,  SPRITE,  "at the sprite threshold"},
        {1.9,  GLINT,   "below sprite, above glint"},
        {0.5,  GLINT,   "at the glint threshold"},
        {0.4,  CULLED,  "below every threshold"},
    };
    // A THREE-WAY check, and the third way is the point. The `want` column is
    // the contract's table as I read it; `zref::part::ladder_want` is the same
    // table written independently as the oracle; the RTL is the third. Two
    // agreeing proves less than it looks — if the oracle and my reading
    // disagree, that is a finding about the contract rather than the block.
    int bad = 0, oracle_disagrees = 0;
    for (const C& c : cases) {
      const Out o = decide(px(c.size), 0, false, false, 0, MESHLET, 0, true);
      const uint8_t oracle = zref::part::ladder_want(
          static_cast<uint16_t>(px(c.size)), 0, false, false, 0);
      if (o.rung != c.want) {
        ++bad;
        std::printf("    band: %.1f px -> %d, wanted %d (%s)\n", c.size, o.rung,
                    c.want, c.why);
      }
      if (static_cast<int>(oracle) != c.want) ++oracle_disagrees;
    }
    zhao::check(bad == 0, "every band picks its own rung, at and either side of "
                          "each threshold", 0, bad);
    zhao::check(oracle_disagrees == 0,
                "and zref::part::ladder_want agrees with the same table, so the "
                "RTL and the oracle are not merely agreeing with each other",
                0, oracle_disagrees);
  }

  // ---- 2: THE OVERLAP resolves coarse-first ------------------------------
  // 7 px is inside triangle (~6-18) AND inside soft sprite (~2-8). The ladder
  // is an order, so it must come out a triangle.
  {
    const Out o = decide(px(7.0), 0, false, false, 0, MESHLET, 0, true);
    zhao::check(o.rung == SHARD,
                "7 px is inside BOTH the triangle and the sprite band, and the "
                "ladder's order resolves it to the coarser one",
                SHARD, o.rung);
  }

  // ---- 3: a streak is chosen on its TRAIL, and only if narrow ------------
  {
    // 3 px round particle with a long trail, species says narrow: a streak.
    const Out a = decide(px(3.0), px(9.0), true, false, 0, MESHLET, 0, true);
    // the same particle, species does NOT say narrow: a sprite, not a streak.
    const Out b = decide(px(3.0), px(9.0), false, false, 0, MESHLET, 0, true);
    zhao::check(a.rung == RIBBON && b.rung == SPRITE,
                "a long trail makes a streak only when the species says it "
                "reads as a line -- a round particle with a long trail is a "
                "round particle that moved",
                1, (a.rung == RIBBON && b.rung == SPRITE) ? 1 : 0);
  }

  // ---- 4: the governor coarsens and never refines ------------------------
  {
    const uint32_t before = top.gov_forced_o;
    // a 40 px particle the governor holds down to a sprite
    const Out a = decide(px(40.0), 0, false, false, SPRITE, MESHLET, 0, true);
    zhao::check(a.rung == SPRITE,
                "the governor can force a particle COARSER than its size asks "
                "for -- the only direction that saves work",
                SPRITE, a.rung);
    zhao::check(top.gov_forced_o == before + 1,
                "and it is counted separately, because this reading of "
                "'consuming governor targets' is an interpretation",
                1, static_cast<int>(top.gov_forced_o - before));

    // a 0.4 px particle with the governor asking for MESHLET: still culled.
    const Out b = decide(px(0.4), 0, false, false, MESHLET, MESHLET, 0, true);
    zhao::check(b.rung == CULLED,
                "and it can never force one FINER -- a governor that could "
                "would be asking for more work under pressure",
                CULLED, b.rung);
  }

  // ---- 5: PROTECTED beats the cull, and beats the governor --------------
  {
    const Out a = decide(px(0.1), 0, false, true, 0, MESHLET, 0, true);
    zhao::check(a.rung == GLINT,
                "a semantically protected particle below every threshold stays "
                "visible as a glint -- the flag is the asset's decision, not an "
                "inference from size",
                GLINT, a.rung);
    // and a governor demanding CULLED does not override the flag
    const Out b = decide(px(0.1), 0, false, true, CULLED, MESHLET, 0, true);
    zhao::check(b.rung == GLINT,
                "and the governor cannot cull it either -- otherwise the flag "
                "would be a suggestion",
                GLINT, b.rung);
  }

  // ---- 6: HYSTERESIS -- a hovering particle does not flicker ------------
  // The case the hold count exists for: a particle sitting on the 18 px
  // threshold, alternating by a hair every frame.
  {
    int rung = MESHLET, hold = 0, flips = 0;
    for (int f = 0; f < 40; ++f) {
      const double size = (f % 2 == 0) ? 18.1 : 17.9;   // a hair either side
      const Out o = decide(px(size), 0, false, false, 0, rung, hold, false);
      if (o.rung != rung) ++flips;
      rung = o.rung;
      hold = o.hold;
    }
    zhao::check(flips == 0,
                "a particle alternating across the threshold every frame never "
                "changes rung -- the hold count resets each time the choice "
                "agrees again, so it never reaches HOLD_FRAMES",
                0, flips);
    zhao::check(top.held_o > 0, "and the suppressions are counted", 1,
                top.held_o > 0 ? 1 : 0);
  }

  // ---- 7: ...but a REAL change still happens, after the hold ------------
  {
    int rung = MESHLET, hold = 0;
    int committed_at = -1;
    for (int f = 0; f < 10; ++f) {
      const Out o = decide(px(3.0), 0, false, false, 0, rung, hold, false);
      if (o.changed && committed_at < 0) committed_at = f;
      rung = o.rung;
      hold = o.hold;
    }
    zhao::check(rung == SPRITE,
                "a particle that really has shrunk DOES change rung -- "
                "hysteresis delays a change, it does not prevent one",
                SPRITE, rung);
    zhao::check(committed_at == 2,
                "and it commits on the third consecutive frame, which is "
                "HOLD_FRAMES",
                2, committed_at);
  }

  // ---- 8: the first decision does not wait -----------------------------
  {
    const Out o = decide(px(3.0), 0, false, false, 0, MESHLET, 0, true);
    zhao::check(o.rung == SPRITE && o.changed == 1,
                "a particle with no hold state yet selects immediately -- "
                "making its first frame wait would show every new particle at "
                "the wrong rung for three frames",
                1, (o.rung == SPRITE && o.changed) ? 1 : 0);
  }

  // ---- 9: PER CAMERA -- one particle, two answers ----------------------
  {
    // the same particle, two cameras with different governor pressure
    const Out a = decide(px(20.0), 0, false, false, 0, MESHLET, 0, true);
    const Out b = decide(px(20.0), 0, false, false, GLINT, MESHLET, 0, true);
    zhao::check(a.rung == MESHLET && b.rung == GLINT,
                "the same particle is a meshlet in one camera and a glint in "
                "the other -- selection is per (particle, camera), and nothing "
                "about the particle itself changed",
                1, (a.rung == MESHLET && b.rung == GLINT) ? 1 : 0);
  }

  std::printf("  %u decisions, %u changes, %u held, %u governor-forced\n",
              top.decisions_o, top.changes_o, top.held_o, top.gov_forced_o);

  return zhao::report_and_exit("part_ladder_directed");
}
