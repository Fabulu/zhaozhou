// measure_tokens_binner.cpp — MEASURE.TOKENS -> GEOM.BINNER, BOTH BLOCKS REAL
// (phase 8, ZH-048 against the phase-6 binner).
//
// WHY THIS FILE EXISTS. GEOM.BINNER landed in phase 6 with a token port and
// nothing to plug into. Its law E says so plainly: MEASURE.TOKENS "is phase 8,
// its contract is still a stub, and no packet layout for `token_grant` exists
// anywhere", so the binner published "the minimum surface that honours the
// ledger" — one combinational request/grant pair — and deliberately did NOT
// invent the Duo fairness split, the token width, the cost model, or the
// return path. Phase 8 has now written all four. This file is where the
// phase-6 guess meets the phase-8 law.
//
// It is the same shape as measure_governor_lod.cpp with one difference that
// matters: GOVERNOR -> LOD needed NO adapter, port for port. This seam needs
// six bindings that do not exist in the binner at all. That asymmetry is the
// finding, and `bind_request` below is written so it cannot be skimmed past.
//
// WHAT IT ESTABLISHES:
//
//   A. THE SEAM CLOSES, CYCLE-ACCURATELY. The binner asserts tok_req_o, a real
//      TOKENS instance answers combinationally, the answer is sampled on the
//      accepting edge exactly as law E specifies, and both blocks take that
//      same edge. A scripted grant cannot show any of this.
//   B. DENIAL IS HONOURED IN GEOMETRY, not just in the token block's own
//      counters: the binner's triangles_culled rises by exactly the number of
//      denials, and the emitted job list shrinks to match.
//   C. THE OTHER VIEW IS UNTOUCHED while this one is starved — charter §9
//      fairness observed from the consumer side rather than from inside the
//      block that implements it.
//   D. THE POOL ONLY EVER DRAINS. This is a real seam defect and it is
//      REPORTED, not worked around. See the block comment at the assertion.
//
// Where the seam is underspecified this file pins TODAY'S behaviour and names
// it, so that ratifying a packet layout turns a line here red rather than
// letting the two sides drift apart quietly.

#define ZHAO_GEOM_DEV_BINNER 1

#include "geom_dev.hpp"
#include "tokens_dev.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

using zhao_geom::BinJob;
using zhao_geom::BinStatus;
using zhao_geom::BinTri;
using zhao_geom::BinnerDev;
using zhao_geom::make_bin_tri;
using zref::Clip;
namespace zm = zref::measure;

const Clip::Viewport kVp{0, 0, 384, 240};
const int kGridW = 24;  // 384 / 16
const int kGridH = 15;  // 240 / 16

// ---------------------------------------------------------------------------
// THE ADAPTER, and every choice inside it
// ---------------------------------------------------------------------------
//
// MEASURE.TOKENS reads a QUALIFIED request: view, class, essential, ladder
// rung, cost, source id. GEOM.BINNER emits a BARE PULSE plus the one field it
// happens to carry. So this is not glue. It is six decisions nobody has
// ratified, and each one names who has to own it.
//
//   req_view      CHOSEN, supplied by the caller. The binner has NO view port
//                 — checked against the module header, not assumed; its only
//                 16-bit identity is tri_src_id_i. So either one binner
//                 instance serves one view (what this test assumes), or a
//                 shared binner needs a view input. That is a block-interface
//                 decision and it is NOT made here.
//   req_class     SOUND. The binner is a geometry block. Class 0 is a fact
//                 about which block this is, not a choice.
//   req_cost      CHOSEN: one token per accepted triangle. The honest
//                 alternative — charge per TILE REFERENCE, which is what the
//                 binner actually costs downstream — is REJECTED because the
//                 reference count is not known until the triangle has been
//                 enumerated, and that happens after the grant. A cost model
//                 that needs the answer before the question cannot be built.
//                 Recorded here because it is a genuine constraint on whoever
//                 writes the cost lane.
//   req_essential CHOSEN true. Law T1 separates essential work from
//                 low-priority refinement and the binner cannot tell them
//                 apart: nothing upstream marks a triangle as refinement.
//                 Everything-essential is the SAFE direction — it never
//                 silently drops work that mattered — and it also means law
//                 T1's reserve is exercised by no producer at all today.
//   req_rep       CHOSEN 0. The ladder rung belongs to TERRAIN.LOD and never
//                 reaches the binner, so den_rep_o reports 0 for every denial
//                 from this producer whatever actually made the triangle.
//   req_src_id    SOUND. The one field that genuinely connects, straight
//                 through.
zm::TokenRequest bind_request(const BinTri& t, int view) {
  zm::TokenRequest r;
  r.valid = true;
  r.view = view;
  r.cls = 0;         // geometry
  r.essential = true;
  r.rep = 0;
  r.cost = 1;
  r.src_id = t.src_id;
  return r;
}

/** A modest spread of triangles built through the real CLIP and SETUP oracles,
 *  so the binner is offered the packet the actual chain would hand it.
 *
 *  Screen coordinates are 8.8 SUBPIXEL (pixels x 256) — the same convention
 *  geom_binner_directed uses. Passing plain pixel numbers here made every
 *  triangle sub-pixel, GEOM.CLIP rejected all ten, and the fixture assertion
 *  below is what caught it rather than a silently empty frame. */
std::vector<BinTri> make_tris(int n) {
  std::vector<BinTri> v;
  for (int i = 0; i < n; ++i) {
    const int32_t x = static_cast<int32_t>((i % 5) * 48 * 256);
    const int32_t y = static_cast<int32_t>((i / 5) * 40 * 256);
    BinTri t;
    if (!make_bin_tri(x, y, x + 40 * 256, y, x, y + 32 * 256, kVp,
                      static_cast<uint16_t>(0x900 + i), &t))
      continue;
    v.push_back(t);
  }
  return v;
}

}  // namespace

int main() {
  Vzhao_measure_tokens tok;
  tokens_test::reset_dut(tok);

  // Geometry view 0 gets room for exactly six triangles; view 1 is loaded to
  // the same figure purely so that "untouched" means something at the end.
  zm::TokenBudgets b;
  b.geom[0] = 6;
  b.geom[1] = 6;
  b.frag[0] = 6;
  b.frag[1] = 6;
  b.shared = 0;  // no emergency pool: this test is about ONE named pool
  {
    tokens_test::Stim s;
    s.budget_valid = true;
    s.budgets = b;
    tokens_test::cycle(tok, s);
  }
  const tokens_test::Pools loaded = tokens_test::pools(tok);
  zhao::check(loaded.g0 == 6, "budget reached geometry pool 0", 6, loaded.g0);

  BinnerDev dev;
  const std::vector<BinTri> tris = make_tris(10);
  zhao::check(tris.size() == 10, "ten triangles survived CLIP and SETUP", 10, tris.size());

  // ---- the live seam -------------------------------------------------------
  int granted = 0;
  int denied = 0;
  dev.token_authority = [&](const BinTri& t) {
    const zm::TokenRequest req = bind_request(t, /*view=*/0);
    // The COMBINATIONAL half only: drive the inputs, settle, read the answer.
    // The edge belongs to `after_edge`, so both blocks commit on the same one.
    // That ordering is law E ("sampled on that same edge") expressed as code.
    tok.budget_valid_i = 0;
    tok.ret_valid_i = 0;
    tok.req_valid_i = 1;
    tok.req_view_i = static_cast<uint8_t>(req.view & 1);
    tok.req_class_i = static_cast<uint8_t>(req.cls & 1);
    tok.req_essential_i = req.essential ? 1 : 0;
    tok.req_rep_i = static_cast<uint8_t>(req.rep & 7);
    tok.req_cost_i = req.cost;
    tok.req_src_id_i = req.src_id;
    tok.eval();
    const bool g = tok.tok_grant_o != 0;
    if (g) {
      ++granted;
    } else {
      ++denied;
    }
    return g;
  };
  dev.after_edge = [&]() {
    zhao::tick(tok);  // the SAME edge the binner just took: TOKENS debits here
    tok.req_valid_i = 0;
    tok.eval();
  };

  BinStatus st;
  std::string err;
  const std::vector<BinJob> jobs = dev.frame(tris, kGridW, kGridH, 0, &st, &err);
  zhao::check(err.empty(), err.empty() ? "the seam ran clean" : err.c_str(), 0,
              err.empty() ? 0 : 1);

  // ---- A: the seam closed --------------------------------------------------
  zhao::check(granted + denied == static_cast<int>(tris.size()),
              "every offered triangle reached the token authority exactly once",
              static_cast<uint64_t>(tris.size()), static_cast<uint64_t>(granted + denied));

  // ---- B: denial is honoured, in geometry ---------------------------------
  zhao::check(granted == 6, "exactly the budget was granted", 6, static_cast<uint64_t>(granted));
  zhao::check(denied == 4, "the remainder were denied", 4, static_cast<uint64_t>(denied));
  zhao::check(st.triangles_culled == static_cast<uint32_t>(denied),
              "the binner counted every denial into triangles_culled",
              static_cast<uint64_t>(denied), st.triangles_culled);
  zhao::check(!jobs.empty(), "granted triangles still produced tile jobs", 1,
              jobs.empty() ? 0 : 1);

  const tokens_test::Pools after = tokens_test::pools(tok);

  // ---- C: charter §9 fairness, seen from the consumer side ----------------
  zhao::check(after.g1 == loaded.g1, "the other view's pool is untouched while this one starves",
              loaded.g1, after.g1);

  // ---- D: THE POOL ONLY EVER DRAINS ---------------------------------------
  //
  // A REPORTED SEAM DEFECT, NOT AN ACCEPTED BEHAVIOUR.
  //
  // MEASURE.TOKENS publishes a return path — ret_valid_i, ret_view_i,
  // ret_class_i, ret_shared_i, ret_cost_i — and law T4 gives it meaning.
  // GEOM.BINNER has no return port at all. Law E named the return path as one
  // of the four things it deliberately left for this block to write, and this
  // block wrote it without giving the binner any way to reach it. So a granted
  // triangle's token is gone for good: the geometry pool falls monotonically,
  // and after `budget` triangles this producer is denied forever, however much
  // of that work has actually finished.
  //
  // Nothing here works around it. The assertion pins the behaviour AS IT IS so
  // the fix cannot land quietly — whoever wires a return path will see this
  // line go red and will have to come here and say what the new law is.
  zhao::check(after.g0 == 0,
              "geometry pool 0 is EMPTY: nothing is ever returned (REPORTED DEFECT)", 0, after.g0);

  std::printf("seam: granted=%d denied=%d culled=%u jobs=%zu pools g0=%u g1=%u\n", granted, denied,
              st.triangles_culled, jobs.size(), after.g0, after.g1);
  return zhao::report_and_exit("measure_tokens_binner");
}
