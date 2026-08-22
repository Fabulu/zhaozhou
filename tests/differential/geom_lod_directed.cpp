// geom_lod_directed.cpp — the creature representation ladder, RTL against the
// shipped oracle.
//
// THE ORACLE IS THE SHIPPED FUNCTION, not a restatement of it. `zref::creature::
// lod_raw` and `lod_update` (reference/src/zcreature/creature_sim.cpp:167) are
// what the reference simulation itself calls, and this file drives the RTL and
// those two functions with the same inputs and compares. It never recomputes
// the ladder — the ABS defect in zhao_field_alu is this project's standing
// reminder that a test which restates a law can agree with a wrong
// implementation forever.
//
// WHAT MAKES THIS DIFFERENTIAL WORTH MORE THAN USUAL. The RTL does NOT
// transcribe the reference. The reference divides twice per evaluation; the RTL
// removes both divides using
//
//     floor(N/e) <= T  <=>  N < (T+1)*e        (N >= 0, e > 0)
//     floor(N/e) >= T  <=>  N >= T*e
//
// which is exact but is a genuinely different computation. So the agreement
// being checked here is not "did I retype it correctly" — it is whether an
// algebraic rewrite is truly equivalent, across the boundaries where a rewrite
// like that fails if it is wrong at all. That is why the sweeps below crowd the
// boundaries rather than sampling uniformly:
//
//   1. THE LEGALITY EDGE. err_r == thresh exactly, and one step either side.
//      The `<=` in the reference becomes a `<` against `(thresh+1)*R` in the
//      RTL, and an off-by-one there is invisible everywhere else.
//   2. THE ROUNDING TERM. round_half_up keeps `+ R/2` inside the comparison.
//      Dropping it, or rounding the other way, only shows up on numerators
//      whose remainder sits at exactly half.
//   3. THE 9/11 HYSTERESIS BAND. Coarsening is eager and refining is lazy, and
//      the RTL reaches them through ceil(10*proj/9) and floor(10*proj/11). The
//      seam between them is where an incorrect ceiling shows up.
//   4. e_r == 0, which the reference special-cases to a boundary of zero rather
//      than dividing — the one place the identities do NOT apply, so the RTL
//      must special-case it too.
//   5. THE HOLD. A switch is refused for the first 15 ticks no matter what the
//      geometry says, and `hold` saturates at 0xFFFF rather than wrapping.
//
// DOMAIN. Every input here is non-negative and `bound_radius > 0`, which is the
// block's stated domain and the condition the two identities need (C++ integer
// division truncates toward zero, which equals floor only there). The RTL
// asserts the domain under FORMAL rather than assuming it; this file stays
// inside it deliberately, because outside it the ORACLE is what changes
// meaning, not the RTL.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_geom_lod.h"

#include "zhao_sim.hpp"
#include "zref/zref_creature.hpp"

namespace {

using zhao::check;
namespace zc = zref::creature;

/** A creature type carrying only the four numbers the ladder reads. */
zc::CreatureType make_type(int32_t bound_radius, int32_t micro, int32_t splat, int32_t glint) {
  zc::CreatureType t;
  t.bound_radius = bound_radius;
  t.micro_error = micro;
  t.splat_error = splat;
  t.glint_error = glint;
  return t;
}

/** A type built the way `compile_creature` builds one (core.cpp:568-570). */
zc::CreatureType compiled_type(int32_t bound_radius, int32_t micro) {
  return make_type(bound_radius, micro, bound_radius / 2, bound_radius);
}

void reset_dut(Vzhao_geom_lod& dut) {
  dut.rst_n = 0;
  dut.tick_i = 0;
  dut.proj_radius_q8_i = 0;
  dut.thresh_q8_i = 0;
  dut.bound_radius_i = 1;
  dut.micro_error_i = 0;
  dut.splat_error_i = 0;
  dut.glint_error_i = 0;
  dut.rung_i = 0;
  dut.hold_i = 0;
  dut.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);
}

struct Step {
  uint8_t rung;
  uint16_t hold;
  uint8_t raw;
};

/** One evaluation through the RTL. */
Step dut_step(Vzhao_geom_lod& dut, const zc::CreatureType& t, int32_t proj, int32_t thresh,
              uint8_t rung_in, uint16_t hold_in) {
  dut.proj_radius_q8_i = proj;
  dut.thresh_q8_i = thresh;
  dut.bound_radius_i = t.bound_radius;
  dut.micro_error_i = t.micro_error;
  dut.splat_error_i = t.splat_error;
  dut.glint_error_i = t.glint_error;
  dut.rung_i = rung_in;
  dut.hold_i = hold_in;
  dut.tick_i = 1;
  zhao::tick(dut);
  dut.tick_i = 0;
  dut.eval();
  Step s;
  s.rung = static_cast<uint8_t>(dut.rung_o);
  s.hold = static_cast<uint16_t>(dut.hold_o);
  s.raw = static_cast<uint8_t>(dut.raw_o);
  return s;
}

/** The same evaluation through the shipped functions. */
Step ref_step(const zc::CreatureType& t, int32_t proj, int32_t thresh, uint8_t rung_in,
              uint16_t hold_in) {
  zc::LodState st;
  st.rung = static_cast<zc::LodRung>(rung_in);
  st.hold = hold_in;
  const zc::LodRung raw = zc::lod_raw(proj, thresh, t);
  const zc::LodRung out = zc::lod_update(st, proj, thresh, t);
  Step s;
  s.rung = static_cast<uint8_t>(out);
  s.hold = st.hold;
  s.raw = static_cast<uint8_t>(raw);
  return s;
}

int g_cases = 0;

/** Drive both, compare all three outputs. */
void one(Vzhao_geom_lod& dut, const char* tag, const zc::CreatureType& t, int32_t proj,
         int32_t thresh, uint8_t rung_in, uint16_t hold_in) {
  const Step want = ref_step(t, proj, thresh, rung_in, hold_in);
  const Step got = dut_step(dut, t, proj, thresh, rung_in, hold_in);
  char nm[192];
  std::snprintf(nm, sizeof nm, "%s raw R=%d e=(%d,%d,%d) proj=%d th=%d in=(%u,%u)", tag,
                t.bound_radius, t.micro_error, t.splat_error, t.glint_error, proj, thresh, rung_in,
                hold_in);
  check(got.raw == want.raw, nm, want.raw, got.raw);
  std::snprintf(nm, sizeof nm, "%s rung R=%d e=(%d,%d,%d) proj=%d th=%d in=(%u,%u)", tag,
                t.bound_radius, t.micro_error, t.splat_error, t.glint_error, proj, thresh, rung_in,
                hold_in);
  check(got.rung == want.rung, nm, want.rung, got.rung);
  std::snprintf(nm, sizeof nm, "%s hold R=%d e=(%d,%d,%d) proj=%d th=%d in=(%u,%u)", tag,
                t.bound_radius, t.micro_error, t.splat_error, t.glint_error, proj, thresh, rung_in,
                hold_in);
  check(got.hold == want.hold, nm, want.hold, got.hold);
  ++g_cases;
}

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
  uint64_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
  }
  uint32_t below(uint32_t n) { return n ? static_cast<uint32_t>(next() % n) : 0u; }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  int random_iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && i + 1 < argc) {
      random_iters = std::atoi(argv[++i]);
    }
  }

  Vzhao_geom_lod dut;
  reset_dut(dut);

  // ---- 1. the ladder walks, on a type built the way the compiler builds one -
  // A creature receding from the camera must pass mesh -> micro -> splat ->
  // glint, and the hold must be elapsed or nothing moves at all.
  {
    const zc::CreatureType t = compiled_type(4096, 512);
    for (int32_t proj = 0; proj <= 8192; proj += 97) {
      one(dut, "1.recede", t, proj, 64, 0, 60000);
    }
  }

  // ---- 2. THE LEGALITY EDGE, which is where a `<=` turned into a `<` fails --
  // For each rung, find the threshold at which that rung's error is exactly
  // legal, and test it and both neighbours. The reference asks err <= thresh;
  // the RTL asks proj*e + R/2 < (thresh+1)*R, and an off-by-one between those
  // is invisible unless the comparison is sitting exactly on the boundary.
  {
    const int32_t Rs[] = {1, 2, 3, 255, 256, 257, 4096, 65535};
    const int32_t es[] = {1, 2, 127, 128, 129, 4096};
    for (int32_t R : Rs) {
      for (int32_t e : es) {
        for (int32_t proj : {0, 1, 2, 255, 256, 1000, 65535}) {
          // err = round_half_up(proj*e/R) computed HERE only to aim the sweep;
          // the comparison itself is always RTL vs the shipped function.
          const int64_t err = (static_cast<int64_t>(proj) * e + R / 2) / R;
          for (int64_t d = -2; d <= 2; ++d) {
            const int64_t th = err + d;
            if (th < 0 || th > 0x7FFFFFFF) continue;
            const zc::CreatureType t = make_type(R, e, e, e);
            one(dut, "2.edge", t, proj, static_cast<int32_t>(th), 0, 60000);
          }
        }
      }
    }
  }

  // ---- 3. THE ROUNDING TERM ------------------------------------------------
  // round_half_up keeps `+ R/2` inside the comparison. These pick numerators
  // whose remainder lands exactly on half, where dropping the term or rounding
  // the other way changes the answer and nothing else does.
  {
    for (int32_t R : {2, 4, 6, 10, 100, 1024}) {
      for (int32_t k = 0; k <= 8; ++k) {
        // proj*e chosen so proj*e = k*R + R/2 exactly
        const int32_t e = 1;
        const int32_t proj = k * R + R / 2;
        const zc::CreatureType t = make_type(R, e, e, e);
        for (int32_t th = (k > 0 ? k - 1 : 0); th <= k + 1; ++th) {
          one(dut, "3.round", t, proj, th, 0, 60000);
        }
      }
    }
  }

  // ---- 4. THE HYSTERESIS BAND, both directions ----------------------------
  // Coarsening is eager (10% below the target's boundary) and refining is lazy
  // (10% above the current one's). The RTL reaches both through ceil(10p/9) and
  // floor(10p/11), so the seam is swept a step at a time from every start rung.
  {
    const zc::CreatureType t = compiled_type(4096, 512);
    for (uint8_t start = 0; start <= 3; ++start) {
      for (int32_t proj = 0; proj <= 6000; proj += 13) {
        one(dut, "4.hyst", t, proj, 32, start, 60000);
      }
    }
  }

  // ---- 5. e_r == 0, the one case the identities do NOT cover --------------
  // The reference special-cases a zero error term to a boundary of zero instead
  // of dividing. The RTL must special-case it too; if it instead evaluated the
  // transformed test it would divide by zero or compare against nonsense.
  {
    for (int32_t proj : {0, 1, 2, 1000, 65535}) {
      for (uint8_t start = 0; start <= 3; ++start) {
        for (int32_t th : {0, 1, 1000}) {
          one(dut, "5.zero-all", make_type(1000, 0, 0, 0), proj, th, start, 60000);
          one(dut, "5.zero-micro", make_type(1000, 0, 500, 1000), proj, th, start, 60000);
          one(dut, "5.zero-splat", make_type(1000, 250, 0, 1000), proj, th, start, 60000);
          one(dut, "5.zero-glint", make_type(1000, 250, 500, 0), proj, th, start, 60000);
        }
      }
    }
  }

  // ---- 6. THE MINIMUM HOLD ------------------------------------------------
  // Below 15 ticks NOTHING switches, however far past the boundary the geometry
  // is; at 15 the switch is allowed. A block that dropped the hold looks
  // perfect on every steady-state test and wrong only here.
  {
    const zc::CreatureType t = compiled_type(4096, 512);
    for (uint16_t hold = 0; hold <= 20; ++hold) {
      for (uint8_t start = 0; start <= 3; ++start) {
        one(dut, "6.hold", t, 30000, 8, start, hold);   // wants coarse
        one(dut, "6.hold", t, 1, 20000, start, hold);   // wants fine
      }
    }
    // and the saturation of `hold` itself, which must stick rather than wrap
    for (uint16_t hold : {uint16_t(0xFFFD), uint16_t(0xFFFE), uint16_t(0xFFFF)}) {
      one(dut, "6.hold-sat", t, 100, 1000, 0, hold);
    }
  }

  // ---- 7. degenerate geometry ---------------------------------------------
  // R == 1 is the smallest legal bound radius; equal error terms make several
  // rungs legal at once, which is exactly when "coarsest legal" matters.
  {
    for (int32_t proj : {0, 1, 5, 1000}) {
      for (int32_t th : {0, 1, 5, 1000}) {
        one(dut, "7.R1", make_type(1, 1, 1, 1), proj, th, 0, 60000);
        one(dut, "7.equal", make_type(1000, 700, 700, 700), proj, th, 1, 60000);
        one(dut, "7.rising", make_type(1000, 1, 2, 3), proj, th, 2, 60000);
      }
    }
  }

  // ---- 8. a real walk: state carried forward, tick after tick -------------
  // Everything above hands the block a state and reads one step. This drives a
  // creature toward and away from the camera with the state THREADED, which is
  // how the block is actually used and the only place a hold that resets at the
  // wrong moment shows up.
  {
    const zc::CreatureType t = compiled_type(4096, 512);
    zc::LodState ref_st;
    uint8_t rung = 0;
    uint16_t hold = 0;
    for (int step = 0; step < 400; ++step) {
      const int32_t proj = (step < 200) ? (8000 - step * 40) : ((step - 200) * 40);
      const zc::LodRung want_raw = zc::lod_raw(proj, 48, t);
      const zc::LodRung want = zc::lod_update(ref_st, proj, 48, t);
      const Step got = dut_step(dut, t, proj, 48, rung, hold);
      char nm[128];
      std::snprintf(nm, sizeof nm, "8.walk[%d] proj=%d rung", step, proj);
      check(got.rung == static_cast<uint8_t>(want), nm, static_cast<uint8_t>(want), got.rung);
      std::snprintf(nm, sizeof nm, "8.walk[%d] proj=%d hold", step, proj);
      check(got.hold == ref_st.hold, nm, ref_st.hold, got.hold);
      std::snprintf(nm, sizeof nm, "8.walk[%d] proj=%d raw", step, proj);
      check(got.raw == static_cast<uint8_t>(want_raw), nm, static_cast<uint8_t>(want_raw), got.raw);
      rung = got.rung;
      hold = got.hold;
      ++g_cases;
    }
  }

  // ---- 9. THE OVERFLOW THAT WAS, and the invariant that outlives it -------
  // The random lane below found the reference computing a NEGATIVE rung
  // boundary: `boundary_q8` formed thresh*R/e_r in __int128 and narrowed it to
  // int32, and for a small error term with a large threshold that wraps. A
  // negative boundary makes the eager-coarsen test false for every projected
  // radius, so the ladder refuses to coarsen and a creature stays pinned at a
  // fine rung forever. Owner ruling 2026-08-22: fix the law, never bake the
  // wrap into hardware. The reference now cross-multiplies in __int128 and
  // forms no boundary at all — the same predicate the RTL evaluates.
  //
  // These cases sit ON the old failure and prove it stays fixed.
  {
    // the exact reproducer from the random lane
    one(dut, "9.repro", make_type(59353, 32039, 16833, 1), 339695, 40818, 0, 33);

    // deliberately astride the old int32 boundary: pick thresh*R just below,
    // at, and above 2^31 with a tiny error term, which is where it wrapped.
    const int32_t R = 65535;
    for (int64_t target : {int64_t(2147483646), int64_t(2147483647), int64_t(2147483648),
                           int64_t(2147483649), int64_t(4294967295), int64_t(4294967296)}) {
      const int32_t th = static_cast<int32_t>(target / R);
      for (int32_t e : {1, 2, 3}) {
        for (uint8_t start = 0; start <= 3; ++start) {
          one(dut, "9.astride", make_type(R, e, e, e), 339695, th, start, 60000);
        }
      }
    }

    // the extreme corner: smallest error term, largest radius and threshold
    for (uint8_t start = 0; start <= 3; ++start) {
      one(dut, "9.corner", make_type(2147483647, 1, 1, 1), 2147483647, 2147483647, start, 60000);
      one(dut, "9.corner", make_type(2147483647, 1, 1073741823, 2147483647), 1048576, 1000000,
          start, 60000);
    }
  }

  // ---- 10. MONOTONICITY, which is the law itself and not a comparison -----
  // Every check above compares the RTL against the reference, so both being
  // wrong the same way would pass. This one does not: rung error is
  // PROPORTIONAL to projected radius, so as a creature moves AWAY (proj falls)
  // more rungs become legal and the coarsest legal rung can only get COARSER.
  // The raw rung must therefore be monotonically non-decreasing as proj falls,
  // and the hysteresised rung must be too once the hold is satisfied.
  //
  // This is exactly what the overflow broke — a wrapped boundary made the
  // ladder stop coarsening as the creature receded — and it would have caught
  // it with no oracle at all.
  {
    const zc::CreatureType types[] = {
        compiled_type(4096, 512),
        compiled_type(65535, 1),          // tiny micro_error: the overflow shape
        make_type(59353, 32039, 16833, 1),  // the reproducer's geometry
        make_type(1000, 1, 500, 1000),
    };
    const int32_t threshes[] = {0, 1, 64, 40818, 1000000};
    for (const zc::CreatureType& t : types) {
      for (int32_t th : threshes) {
        uint8_t prev_raw = 0;
        uint8_t prev_rung = 0;
        bool first = true;
        for (int64_t proj = 4000000; proj >= 0; proj -= 9973) {
          const Step s = dut_step(dut, t, static_cast<int32_t>(proj), th, prev_rung, 60000);
          if (!first) {
            char nm[160];
            std::snprintf(nm, sizeof nm,
                          "10.raw never refines as proj falls (R=%d e=(%d,%d,%d) th=%d proj=%lld)",
                          t.bound_radius, t.micro_error, t.splat_error, t.glint_error, th,
                          static_cast<long long>(proj));
            check(s.raw >= prev_raw, nm, prev_raw, s.raw);
            std::snprintf(nm, sizeof nm,
                          "10.rung never refines as proj falls (R=%d e=(%d,%d,%d) th=%d proj=%lld)",
                          t.bound_radius, t.micro_error, t.splat_error, t.glint_error, th,
                          static_cast<long long>(proj));
            check(s.rung >= prev_rung, nm, prev_rung, s.rung);
          }
          prev_raw = s.raw;
          prev_rung = s.rung;
          first = false;
          ++g_cases;
        }
      }
    }
  }

  // ---- 12. THE EXACT BOUNDARIES THE IDENTITIES TURN ON --------------------
  // Two mutants survived the first sweep of this block and NEITHER was
  // equivalent — both were holes in this file, and both are constructed here
  // rather than sampled, because random draws cannot land on a single point.
  //
  //   `<` widened to `<=` on the REFINE test. The identity behind it is
  //   floor(N/e) <= M  <=>  N < (M+1)*e, so the two spellings disagree at
  //   exactly one place: N == (M+1)*e.
  //
  //   the HOLD saturation deleted. That differs only at hold == 0xFFFF, and
  //   only on a path that INCREMENTS the hold. Section 6 drove the rail, but on
  //   inputs that switch rungs — and a switch clears the hold to zero, so the
  //   increment never ran at the rail.
  {
    // With R = 1, e = 2k, thresh = k and proj = 1:
    //   N = thresh*R + e/2 = 2k,  M = floor(10*1/11) = 0,  (M+1)*e = 2k.
    // Exactly the boundary. The refine branch needs raw < rung_i, so the glint
    // rung (e = 2k) must be ILLEGAL while a finer one is legal — which is why
    // micro and splat are zero here and glint carries the whole error.
    for (int32_t k = 1; k <= 8; ++k) {
      one(dut, "12.refine-exact", make_type(1, 0, 0, 2 * k), 1, k, 3, 60000);
    }

    // The coarsening twin: N == K*e with K = ceil(10*proj/9), which is 0 at
    // proj = 0, so N must be 0 too — thresh = 0 and an error term of 1.
    for (int32_t R : {2, 3, 100, 4096}) {
      one(dut, "12.coarsen-exact", make_type(R, 1, 1, 1), 0, 0, 0, 60000);
    }

    // The hold rail on a STAY path: rung_i == raw, so the block increments
    // instead of clearing, and 0xFFFF must stick rather than wrap to zero.
    {
      const zc::CreatureType t = compiled_type(4096, 512);
      for (uint16_t hold : {uint16_t(0xFFFD), uint16_t(0xFFFE), uint16_t(0xFFFF)}) {
        one(dut, "12.hold-rail-stay", t, 100, 1000, 3, hold);  // raw == 3 == rung_i
      }
      // and on a REFUSED switch, which also increments rather than clearing
      for (uint16_t hold : {uint16_t(0xFFFE), uint16_t(0xFFFF)}) {
        one(dut, "12.hold-rail-refused", t, 4000, 64, 1, hold);
      }
    }
  }

  // ---- 13. random over the whole domain -----------------------------------
  if (random_iters > 0) {
    Prng rng(0x10D5EEDu);
    for (int it = 0; it < random_iters; ++it) {
      const int32_t R = 1 + static_cast<int32_t>(rng.below(65535));
      // half the draws are compiler-shaped types, half are arbitrary — the
      // first is what the machine will see, the second is what keeps the block
      // from quietly depending on splat == R/2 and glint == R.
      zc::CreatureType t = (it & 1) ? compiled_type(R, static_cast<int32_t>(rng.below(R + 1)))
                                    : make_type(R, static_cast<int32_t>(rng.below(R + 1)),
                                                static_cast<int32_t>(rng.below(R + 1)),
                                                static_cast<int32_t>(rng.below(R + 1)));
      const int32_t proj = static_cast<int32_t>(rng.below(1u << 20));
      const int32_t th = static_cast<int32_t>(rng.below(1u << 16));
      const uint8_t start = static_cast<uint8_t>(rng.below(4));
      const uint16_t hold = static_cast<uint16_t>(rng.below(40));
      one(dut, "13.random", t, proj, th, start, hold);
    }
    std::printf("geom_lod random: %d evaluations\n", random_iters);
  }

  std::printf("geom_lod: %d evaluations compared\n", g_cases);
  return zhao::report_and_exit("geom_lod_directed");
}
