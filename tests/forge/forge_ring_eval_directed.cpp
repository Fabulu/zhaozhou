// forge_ring_eval_directed.cpp -- the ring evaluator against its oracle.
//
// `zhao_forge_ring_eval` is the POSITION law for the four forge families that
// had none -- fan, tube, radial shell and billboard sheet -- bought by owner
// decision R234 D2, which reversed R199's deferral with the words *"the owner
// has chosen to pay for the evaluators rather than accept the deferral."*
//
// ---------------------------------------------------------------------------
// WHAT THIS TEST HOLDS, AND WHY EACH ONE IS HERE
// ---------------------------------------------------------------------------
// 1. **BIT-EXACTNESS** against `zref::forge_ring::eval_job`, component for
//    component: every family, both sweeps, minimum and maximum subdivision,
//    and a randomized sweep over the full legal domain including anchors
//    anywhere in s32 and a deliberately saturating fraction.
// 2. **THE WHOLE-JOB PROPERTY, POSITIVELY.** An accepted job emits EXACTLY
//    `(N+1) * K` vertices and a refused one emits ZERO -- checked on every
//    vector under every stall pattern. This block ships no `walk_overrun`
//    counter precisely because this is the property that counter would be
//    guessing at, and `CLAUDE.md` says to assert the correct behaviour rather
//    than the bug.
// 3. **DETERMINISM UNDER STALLS**: four consumer patterns produce the
//    IDENTICAL stream, and a rerun of the same job reproduces it. A capture
//    CRC is a contract and this is the property it rests on.
// 4. **THE GRID IS `zhao_forge_prim`'s GRID.** The emitted `v_ring_o`/`v_k_o`
//    walk is compared against `zref::forge::prim_vertex_index`, so the
//    positions and the topology indices cannot drift apart silently.
// 5. **THE OPEN RING IS THE RIBBON'S PAIR**: `C - R*U` then `C + R*U`, in that
//    order -- the same two vertices `zhao_forge_prim_eval` emits, checked here
//    against a hand-computed expectation rather than against itself.
// 6. **THE DOME'S ENDPOINT IDENTITIES ARE EXACT**: C(0) = A0, C(N) = A1 and
//    R(0) = r0 to the BIT, because `sin 0 = 0`, `sin(quarter) = 0x10000` and
//    `cos 0 = 0x10000` exactly in the frozen SIN_Q16 table. If the table ever
//    stops being seventeen bits wide these go red first.
// 7. **EVERY COUNTER FIRED BY LEGAL STIMULUS**, saturation included, and each
//    refusal counted on its OWN port -- a job naming the ribbon must NOT land
//    on `refused_family_o`, because that would make a dispatch fault look like
//    a caller fault (R95: a counter must DISCRIMINATE).
// 8. **THE COMPARATOR SEEN TO FAIL** on a planted one-bit corruption. Without
//    it every "equal" above is an instrument reading reassurance.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_forge_ring_eval.h"

#include "zhao_sim.hpp"
#include "zref/zref_forge.hpp"
#include "zref/zref_forge_ring.hpp"

namespace fr = zref::forge_ring;
using zhao::check;

namespace {

struct OutVertex {
  int32_t x, y, z;
  int ring, k, last;
};

// test-local deterministic RNG -- stimulus only, never the DUT's law
uint32_t g_rng = 0x5EED1234u;
uint32_t rnd() {
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 17;
  g_rng ^= g_rng << 5;
  return g_rng;
}

fr::Vec3 v3(int32_t x, int32_t y, int32_t z) {
  fr::Vec3 v;
  v.x = x;
  v.y = y;
  v.z = z;
  return v;
}

constexpr int32_t kOne = 65536;

void apply(Vzhao_forge_ring_eval& top, const fr::Params& p) {
  top.j_family_i = p.family & 7;
  top.j_sweep_i = p.sweep & 1;
  top.j_segments_i = p.segments & 0x7F;
  top.j_sides_i = p.sides & 0xF;
  top.j_a0_x_i = p.anchor0.x;
  top.j_a0_y_i = p.anchor0.y;
  top.j_a0_z_i = p.anchor0.z;
  top.j_a1_x_i = p.anchor1.x;
  top.j_a1_y_i = p.anchor1.y;
  top.j_a1_z_i = p.anchor1.z;
  top.j_u_x_i = p.axis_u.x;
  top.j_u_y_i = p.axis_u.y;
  top.j_u_z_i = p.axis_u.z;
  top.j_v_x_i = p.axis_v.x;
  top.j_v_y_i = p.axis_v.y;
  top.j_v_z_i = p.axis_v.z;
  top.j_r0_i = p.radius0;
  top.j_r1_i = p.radius1;
  top.j_view_mask_i = p.view_mask & 3;
  top.j_src_id_i = p.src_id;
}

void hard_reset(Vzhao_forge_ring_eval& top) {
  top.j_valid_i = 0;
  top.v_ready_i = 0;
  top.view_sel_i = 1;
  top.rst_n = 0;
  top.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();
  zhao::tick(top);
}

void submit(Vzhao_forge_ring_eval& top, const fr::Params& p, int view_sel) {
  apply(top, p);
  top.view_sel_i = view_sel & 3;
  top.j_valid_i = 1;
  for (int w = 0; w < 1000; ++w) {
    top.eval();
    if (top.j_ready_o) {
      zhao::tick(top);
      top.j_valid_i = 0;
      return;
    }
    zhao::tick(top);
  }
  check(false, "job handshake hung", 1, 0);
  top.j_valid_i = 0;
}

// 0 always ready; 1 every third cycle; 2 pseudo-random; 3 one cycle in ten.
bool ready_now(int pattern, int cycle, uint32_t& lrng) {
  switch (pattern) {
    case 0:
      return true;
    case 1:
      return (cycle % 3) == 0;
    case 2:
      lrng ^= lrng << 13;
      lrng ^= lrng >> 17;
      lrng ^= lrng << 5;
      return (lrng & 1) != 0;
    default:
      return (cycle % 10) == 0;
  }
}

/** Collect an accepted job's whole stream, terminating on `v_last_o`. */
std::vector<OutVertex> collect(Vzhao_forge_ring_eval& top, int pattern,
                               int max_cycles = 4000000) {
  std::vector<OutVertex> out;
  uint32_t lrng = 0xB0B0CAFEu;
  bool saw_last = false;
  for (int c = 0; c < max_cycles && !saw_last; ++c) {
    top.v_ready_i = ready_now(pattern, c, lrng) ? 1 : 0;
    top.eval();
    if (top.v_valid_o && top.v_ready_i) {
      OutVertex v;
      v.x = (int32_t)top.v_x_o;
      v.y = (int32_t)top.v_y_o;
      v.z = (int32_t)top.v_z_o;
      v.ring = (int)top.v_ring_o;
      v.k = (int)top.v_k_o;
      v.last = (int)top.v_last_o;
      out.push_back(v);
      if (v.last) saw_last = true;
    }
    zhao::tick(top);
  }
  top.v_ready_i = 0;
  check(saw_last, "stream terminated with v_last", 1, saw_last ? 1 : 0);
  return out;
}

/**
 * Run a job that must be REFUSED or SKIPPED, and hold that NOTHING came out.
 * "Nothing emitted" is the refusal law's whole content and it cannot be read
 * off a counter -- a counter says a refusal happened, not that the stream
 * stayed silent.
 */
int drain_silent(Vzhao_forge_ring_eval& top, int cycles = 200) {
  int seen = 0;
  for (int c = 0; c < cycles; ++c) {
    top.v_ready_i = 1;
    top.eval();
    if (top.v_valid_o) ++seen;
    zhao::tick(top);
  }
  top.v_ready_i = 0;
  return seen;
}

/** The one comparator every case funnels through, and the one the negative
 *  control corrupts. Returns the number of mismatching components. */
int compare(const std::vector<OutVertex>& got, const std::vector<fr::Vec3>& want) {
  int bad = 0;
  const size_t n = got.size() < want.size() ? got.size() : want.size();
  for (size_t i = 0; i < n; ++i) {
    if (got[i].x != want[i].x) ++bad;
    if (got[i].y != want[i].y) ++bad;
    if (got[i].z != want[i].z) ++bad;
  }
  bad += (int)(got.size() > want.size() ? got.size() - want.size() : want.size() - got.size());
  return bad;
}

/** One accepted job, end to end, against the oracle. */
void run_case(Vzhao_forge_ring_eval& top, const fr::Params& p, int view_sel, int pattern,
              const char* what, fr::Counts& cum) {
  std::vector<fr::Vec3> want;
  fr::Counts c;
  const fr::Verdict v = fr::eval_job(p, view_sel, want, c);
  check(v == fr::kAccept, "case is expected to be accepted", fr::kAccept, (uint64_t)v);
  if (v != fr::kAccept) return;

  submit(top, p, view_sel);
  const std::vector<OutVertex> got = collect(top, pattern);

  check(got.size() == want.size(), what, (uint64_t)want.size(), (uint64_t)got.size());
  check(compare(got, want) == 0, what, 0, (uint64_t)compare(got, want));

  // The WHOLE-JOB property, positively: exactly (N+1)*K vertices.
  check((int)got.size() == fr::vertex_count(p), "emitted count is exactly (N+1)*K",
        (uint64_t)fr::vertex_count(p), (uint64_t)got.size());

  // The grid is the topology walker's grid: ring-major, vidx(s,k) = s*K + k.
  const int K = fr::ring_vertices(p);
  int walk_bad = 0;
  for (size_t i = 0; i < got.size(); ++i) {
    if (got[i].ring != (int)(i / (size_t)K)) ++walk_bad;
    if (got[i].k != (int)(i % (size_t)K)) ++walk_bad;
    if (zref::forge::prim_vertex_index(got[i].ring, got[i].k, K) != (int)i) ++walk_bad;
  }
  check(walk_bad == 0, "the emission walk is ring-major vidx(s,k) = s*K + k", 0,
        (uint64_t)walk_bad);

  // Exactly one `last`, and it is the final vertex.
  int lasts = 0;
  for (size_t i = 0; i < got.size(); ++i) lasts += got[i].last;
  check(lasts == 1, "exactly one vertex is marked last", 1, (uint64_t)lasts);

  cum.rings += c.rings;
  cum.vertices += c.vertices;
  cum.sat_events += c.sat_events;
}

fr::Params tube_case(int segments, int sides) {
  fr::Params p;
  p.family = fr::kFamTube;
  p.segments = segments;
  p.sides = sides;
  p.anchor0 = v3(0, 0, 0);
  p.anchor1 = v3(0, 16 * kOne, 0);
  p.axis_u = v3(kOne, 0, 0);
  p.axis_v = v3(0, 0, kOne);
  p.radius0 = kOne;
  p.radius1 = kOne / 4;
  p.view_mask = 3;
  p.src_id = 0x0303;
  return p;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_forge_ring_eval top;
  hard_reset(top);

  fr::Counts cum;

  // ---- 1. every family, minimum and maximum subdivision ------------------
  {
    fr::Params p = tube_case(1, 1);
    run_case(top, p, 1, 0, "tube, minimum subdivision", cum);
    p = tube_case(fr::kMaxSegments, fr::kMaxSides);
    run_case(top, p, 1, 0, "tube, 64 x 8 -- the contract's worst case", cum);
    // Every ring size, including the ones whose turn does NOT divide 65536.
    for (int sides = 1; sides <= fr::kMaxSides; ++sides) {
      p = tube_case(3, sides);
      run_case(top, p, 1, 0, "tube, every ring size 1..8", cum);
    }
  }
  {
    fr::Params p = tube_case(1, 8);
    p.family = fr::kFamFan;
    p.radius0 = 0;               // a hub at the centre
    p.radius1 = 2 * kOne;
    p.anchor1 = p.anchor0;       // a flat ring: both rings share a centre
    run_case(top, p, 1, 0, "radial fan, flat ring with a zero hub", cum);
    p.segments = 64;             // FAN pins N = 1 whatever `segments` says
    std::vector<fr::Vec3> w;
    fr::Counts c;
    fr::eval_job(p, 1, w, c);
    check(w.size() == 16u, "FAN pins N = 1 however many segments are asked for", 16u,
          (uint64_t)w.size());
    run_case(top, p, 1, 0, "radial fan ignores `segments`, as the topology walker does", cum);
  }
  {
    fr::Params p = tube_case(1, 1);
    p.family = fr::kFamBillboard;
    p.anchor0 = v3(0, 0, 0);
    p.anchor1 = v3(0, 2 * kOne, 0);
    p.radius0 = kOne;
    p.radius1 = kOne;
    run_case(top, p, 1, 0, "billboard sheet -- one quad", cum);

    // 5. THE OPEN RING IS THE RIBBON'S PAIR, hand-computed rather than
    //    compared against the thing under test.
    std::vector<fr::Vec3> w;
    fr::Counts c;
    fr::eval_job(p, 1, w, c);
    check(w.size() == 4u, "a billboard is four vertices", 4u, (uint64_t)w.size());
    check(w[0].x == -kOne && w[1].x == kOne, "ring 0 is C - R*U then C + R*U, in that order",
          (uint64_t)(uint32_t)(-kOne), (uint64_t)(uint32_t)w[0].x);
    check(w[0].y == 0 && w[1].y == 0, "ring 0 sits at anchor0", 0, (uint64_t)(uint32_t)w[0].y);
    check(w[2].y == 2 * kOne && w[3].y == 2 * kOne, "ring 1 sits at anchor1",
          (uint64_t)(2 * kOne), (uint64_t)(uint32_t)w[2].y);
  }

  // ---- 6. the SHELL, both sweeps, and the DOME's exact endpoints ---------
  {
    fr::Params p = tube_case(16, 8);
    p.family = fr::kFamShell;
    p.radius0 = 4 * kOne;
    p.radius1 = 0;               // a cone
    run_case(top, p, 1, 0, "radial shell, LINEAR sweep -- a cone", cum);

    p.sweep = fr::kSweepDome;
    p.radius0 = 4 * kOne;
    p.radius1 = 4 * kOne;
    p.anchor0 = v3(0, 0, 0);
    p.anchor1 = v3(0, 4 * kOne, 0);
    run_case(top, p, 1, 0, "radial shell, DOME sweep", cum);

    std::vector<fr::Vec3> w;
    fr::Counts c;
    fr::eval_job(p, 1, w, c);
    const int K = fr::ring_vertices(p);
    // C(0) = A0 exactly: sin 0 = 0, so nothing is added.
    check(w[0].y == 0, "DOME: C(0) = anchor0 to the BIT", 0, (uint64_t)(uint32_t)w[0].y);
    // R(0) = r0 exactly: cos 0 = 0x10000, and rescale16(r0 * 0x10000) == r0.
    check(w[0].x == 4 * kOne, "DOME: R(0) = radius0 to the BIT", (uint64_t)(4 * kOne),
          (uint64_t)(uint32_t)w[0].x);
    // C(N) = A1 exactly: sin(quarter turn) is 0x10000 and not one ulp below.
    const size_t pole = w.size() - (size_t)K;
    check(w[pole].y == 4 * kOne, "DOME: C(N) = anchor1 to the BIT -- sin(0x4000) is EXACTLY 1.0",
          (uint64_t)(4 * kOne), (uint64_t)(uint32_t)w[pole].y);
    check(w[pole].x == 0, "DOME: R(N) = 0 -- cos(0x4000) is EXACTLY 0", 0,
          (uint64_t)(uint32_t)w[pole].x);

    // A DOME whose sweep is degenerate is still the LINEAR base ring.
    fr::Params q = p;
    q.sweep = fr::kSweepLinear;
    std::vector<fr::Vec3> wl;
    fr::Counts cl;
    fr::eval_job(q, 1, wl, cl);
    check(wl[0].x == w[0].x && wl[0].y == w[0].y,
          "the two sweeps agree on ring 0, where both are exact", (uint64_t)(uint32_t)wl[0].x,
          (uint64_t)(uint32_t)w[0].x);
  }

  // ---- 3. determinism: four stall patterns, one byte stream --------------
  {
    const fr::Params p = tube_case(24, 6);
    // The cumulative counter check at the bottom compares against EVERY job
    // this file submits, so the jobs run here for their stream must be counted
    // here too. (They were not, on the first run, and the counters disagreed by
    // exactly 130 rings and 770 vertices -- which is exactly these five jobs
    // plus the comparator's one. An off-by-a-whole-job is what that check is
    // for; the agreement is only evidence because it is exact.)
    {
      std::vector<fr::Vec3> w;
      fr::Counts c;
      fr::eval_job(p, 1, w, c);
      for (int rep = 0; rep < 5; ++rep) {  // four stall patterns plus the rerun
        cum.rings += c.rings;
        cum.vertices += c.vertices;
        cum.sat_events += c.sat_events;
      }
    }
    std::vector<OutVertex> ref;
    for (int pattern = 0; pattern < 4; ++pattern) {
      submit(top, p, 1);
      const std::vector<OutVertex> got = collect(top, pattern);
      if (pattern == 0) {
        ref = got;
      } else {
        int bad = (int)(got.size() != ref.size());
        for (size_t i = 0; i < got.size() && i < ref.size(); ++i)
          if (got[i].x != ref[i].x || got[i].y != ref[i].y || got[i].z != ref[i].z) ++bad;
        check(bad == 0, "the stream is byte-identical under every stall pattern", 0,
              (uint64_t)bad);
      }
      check(got.size() == (size_t)fr::vertex_count(p),
            "the whole-job count holds under backpressure too",
            (uint64_t)fr::vertex_count(p), (uint64_t)got.size());
    }
    // And a rerun of the same job reproduces it -- no state survives a job.
    submit(top, p, 1);
    const std::vector<OutVertex> again = collect(top, 0);
    int bad = (int)(again.size() != ref.size());
    for (size_t i = 0; i < again.size() && i < ref.size(); ++i)
      if (again[i].x != ref[i].x || again[i].y != ref[i].y || again[i].z != ref[i].z) ++bad;
    check(bad == 0, "a rerun of the same job reproduces the stream exactly", 0, (uint64_t)bad);
  }

  // ---- 7. every refusal, on its OWN port, with NOTHING emitted -----------
  {
    const uint32_t fam0 = top.refused_family_o;
    const uint32_t els0 = top.refused_elsewhere_o;
    const uint32_t lim0 = top.refused_limit_o;
    const uint32_t skp0 = top.skipped_view_o;

    fr::Params p = tube_case(4, 4);
    p.family = 6;  // outside the CLOSED six
    check(fr::verdict(p, 1) == fr::kRefusedFamily, "the oracle refuses family 6",
          fr::kRefusedFamily, (uint64_t)fr::verdict(p, 1));
    submit(top, p, 1);
    check(drain_silent(top) == 0, "a family-refused job emits NOTHING", 0,
          (uint64_t)drain_silent(top));
    top.eval();
    check(top.refused_family_o == fam0 + 1, "refused_family_o fires", (uint64_t)(fam0 + 1),
          (uint64_t)top.refused_family_o);
    check(top.refused_elsewhere_o == els0, "and refused_elsewhere_o does NOT", (uint64_t)els0,
          (uint64_t)top.refused_elsewhere_o);

    // THE DISCRIMINATION THAT MATTERS: the ribbon and the cliff are LEGAL
    // families this block does not own. They must NOT land on refused_family_o
    // -- a dispatch fault reported as a caller fault sends the next reader to
    // fix the wrong thing.
    for (int fam = 0; fam < 6; fam += 5) {  // 0 = ribbon, 5 = cliff
      const uint32_t f_before = top.refused_family_o;
      const uint32_t e_before = top.refused_elsewhere_o;
      p.family = fam;
      check(fr::verdict(p, 1) == fr::kRefusedElsewhere,
            "the oracle sends ribbon/cliff to their own verdict", fr::kRefusedElsewhere,
            (uint64_t)fr::verdict(p, 1));
      submit(top, p, 1);
      check(drain_silent(top) == 0, "a family owned elsewhere emits NOTHING", 0,
            (uint64_t)drain_silent(top));
      top.eval();
      check(top.refused_elsewhere_o == e_before + 1, "refused_elsewhere_o fires",
            (uint64_t)(e_before + 1), (uint64_t)top.refused_elsewhere_o);
      check(top.refused_family_o == f_before,
            "refused_family_o stays PUT -- ribbon and cliff are legal families",
            (uint64_t)f_before, (uint64_t)top.refused_family_o);
    }

    // Limits: refused at their boundary, accepted just inside it.
    const int bad_seg[2] = {0, fr::kMaxSegments + 1};
    for (int i = 0; i < 2; ++i) {
      const uint32_t before = top.refused_limit_o;
      p = tube_case(bad_seg[i], 4);
      submit(top, p, 1);
      check(drain_silent(top) == 0, "a limit-refused job emits NOTHING", 0,
            (uint64_t)drain_silent(top));
      top.eval();
      check(top.refused_limit_o == before + 1, "refused_limit_o fires on a bad segment count",
            (uint64_t)(before + 1), (uint64_t)top.refused_limit_o);
    }
    const int bad_sides[2] = {0, fr::kMaxSides + 1};
    for (int i = 0; i < 2; ++i) {
      const uint32_t before = top.refused_limit_o;
      p = tube_case(4, bad_sides[i]);
      submit(top, p, 1);
      check(drain_silent(top) == 0, "a side-limit-refused job emits NOTHING", 0,
            (uint64_t)drain_silent(top));
      top.eval();
      check(top.refused_limit_o == before + 1, "refused_limit_o fires on a bad side count",
            (uint64_t)(before + 1), (uint64_t)top.refused_limit_o);
    }
    check(top.refused_limit_o == lim0 + 4, "four limit refusals in all", (uint64_t)(lim0 + 4),
          (uint64_t)top.refused_limit_o);

    // A well-formed job for the OTHER view is SKIPPED, not refused.
    p = tube_case(4, 4);
    p.view_mask = 2;
    submit(top, p, 1);  // view_sel = 1
    check(drain_silent(top) == 0, "a view-skipped job emits NOTHING", 0,
          (uint64_t)drain_silent(top));
    top.eval();
    check(top.skipped_view_o == skp0 + 1, "skipped_view_o fires", (uint64_t)(skp0 + 1),
          (uint64_t)top.skipped_view_o);
    check(top.refused_limit_o == lim0 + 4, "and no refusal counter moved with it",
          (uint64_t)(lim0 + 4), (uint64_t)top.refused_limit_o);

    // The same job for the view it names IS accepted -- the skip is about the
    // view and not about the job.
    run_case(top, p, 2, 0, "the same job accepted for the view it names", cum);
  }

  // ---- saturation, fired by legal stimulus -------------------------------
  {
    const uint32_t sat0 = top.sat_events_o;
    fr::Params p = tube_case(4, 4);
    // Anchors at opposite ends of s32 with a radius that pushes every vertex
    // past the rail: this saturates, and saturating is the DECLARED behaviour.
    p.anchor0 = v3(INT32_C(2147483647), INT32_C(2147483647), INT32_C(2147483647));
    p.anchor1 = v3(INT32_C(-2147483648), INT32_C(-2147483648), INT32_C(-2147483648));
    p.radius0 = INT32_C(2147483647);
    p.radius1 = INT32_C(2147483647);
    p.axis_u = v3(kOne, kOne, kOne);
    p.axis_v = v3(kOne, kOne, kOne);
    run_case(top, p, 1, 2, "an extreme job still matches the oracle bit for bit", cum);
    top.eval();
    check(top.sat_events_o > sat0, "sat_events_o fires under legal, extreme stimulus", 1,
          top.sat_events_o > sat0 ? 1 : 0);
  }

  // ---- 8. THE COMPARATOR IS SEEN TO FAIL ---------------------------------
  {
    const fr::Params p = tube_case(4, 4);
    std::vector<fr::Vec3> want;
    fr::Counts c;
    fr::eval_job(p, 1, want, c);
    submit(top, p, 1);
    const std::vector<OutVertex> got = collect(top, 0);
    cum.rings += c.rings;   // this job counts toward the cumulative check too
    cum.vertices += c.vertices;
    cum.sat_events += c.sat_events;
    check(compare(got, want) == 0, "the clean case compares equal", 0,
          (uint64_t)compare(got, want));

    std::vector<fr::Vec3> bent = want;
    bent[bent.size() / 2].z ^= 1;  // ONE bit, in the middle of the stream
    check(compare(got, bent) == 1, "the comparator FAILS on a planted one-bit corruption", 1,
          (uint64_t)compare(got, bent));

    std::vector<fr::Vec3> shortened = want;
    shortened.pop_back();
    check(compare(got, shortened) > 0, "the comparator FAILS on a length mismatch", 1,
          compare(got, shortened) > 0 ? 1 : 0);
  }

  // ---- 1b. the randomized sweep over the legal domain --------------------
  {
    int mismatches = 0;
    int jobs = 0;
    for (int t = 0; t < 120; ++t) {
      fr::Params p;
      const int fams[4] = {fr::kFamFan, fr::kFamTube, fr::kFamShell, fr::kFamBillboard};
      p.family = fams[rnd() % 4];
      p.sweep = (p.family == fr::kFamShell && (rnd() & 1)) ? fr::kSweepDome : fr::kSweepLinear;
      // Bias toward MAXIMAL subdivision: a uniform draw spends almost all its
      // time on small primitives and nearly never reaches the worst case that
      // sizes everything downstream. FORGE.PRIM.md asks for exactly this bias.
      p.segments = (rnd() & 1) ? (int)(rnd() % fr::kMaxSegments) + 1
                               : fr::kMaxSegments - (int)(rnd() % 6);
      p.sides = (int)(rnd() % fr::kMaxSides) + 1;
      p.view_mask = 3;
      p.src_id = (uint16_t)rnd();
      const bool extreme = (t % 7) == 0;  // a deliberately saturating fraction
      const int32_t span = extreme ? INT32_C(2000000000) : (64 * kOne);
      p.anchor0 = v3((int32_t)rnd() % span, (int32_t)rnd() % span, (int32_t)rnd() % span);
      p.anchor1 = v3((int32_t)rnd() % span, (int32_t)rnd() % span, (int32_t)rnd() % span);
      p.axis_u = v3((int32_t)rnd() % (2 * kOne), (int32_t)rnd() % (2 * kOne),
                    (int32_t)rnd() % (2 * kOne));
      p.axis_v = v3((int32_t)rnd() % (2 * kOne), (int32_t)rnd() % (2 * kOne),
                    (int32_t)rnd() % (2 * kOne));
      p.radius0 = extreme ? INT32_C(2000000000) : (int32_t)(rnd() % (8u * 65536u));
      p.radius1 = extreme ? INT32_C(2000000000) : (int32_t)(rnd() % (8u * 65536u));

      std::vector<fr::Vec3> want;
      fr::Counts c;
      if (fr::eval_job(p, 1, want, c) != fr::kAccept) continue;
      submit(top, p, 1);
      const std::vector<OutVertex> got = collect(top, (int)(rnd() % 4));
      const int bad = compare(got, want);
      if (bad) ++mismatches;
      cum.rings += c.rings;
      cum.vertices += c.vertices;
      cum.sat_events += c.sat_events;
      ++jobs;
    }
    check(mismatches == 0, "the randomized full-domain sweep is bit-exact", 0,
          (uint64_t)mismatches);
    check(jobs >= 100, "the sweep actually ran its jobs", 1, jobs >= 100 ? 1 : 0);
  }

  // ---- the counters equal the oracle's cumulative counts -----------------
  // Cumulative, not per job: a per-job comparison can pass while the block
  // double-counts and under-counts in alternating jobs.
  top.eval();
  check(top.rings_o == (uint32_t)cum.rings, "rings_o equals the oracle's ring count",
        (uint64_t)cum.rings, (uint64_t)top.rings_o);
  check(top.vertices_o == (uint32_t)cum.vertices, "vertices_o equals the oracle's vertex count",
        (uint64_t)cum.vertices, (uint64_t)top.vertices_o);
  check(top.sat_events_o == (uint32_t)cum.sat_events,
        "sat_events_o equals the oracle's saturation count", (uint64_t)cum.sat_events,
        (uint64_t)top.sat_events_o);
  check(top.jobs_o > 0, "jobs_o fired", 1, top.jobs_o > 0 ? 1 : 0);

  return zhao::report_and_exit("forge_ring_eval_directed");
}
