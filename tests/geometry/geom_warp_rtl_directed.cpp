// geom_warp_rtl_directed.cpp -- the RTL directed test for GEOM.WARP.
//
// AUTHORITY. Owner directive reports/Zhaozhou_GEOM_WARP_Architecture_2026-09-20.txt
// (owner commit 4c256137), W01-W18, ratified in
// reports/OWNER-RATIFICATION-20260920-WARP.md. Contract design/contracts/GEOM.WARP.md.
//
// WHAT IT COMPARES AGAINST, AND WHY THAT MATTERS. Every numeric expectation in
// this file comes from calling `zref::geom_warp::apply_outputs` -- the SAME C++
// function the reference test exercises -- with the hardware's own six output
// words. It does NOT restate the application law in C++ and then check the RTL
// against the restatement. A restatement is a second implementation, and two
// implementations written by the same hand on the same afternoon agree on
// exactly the cases the author thought of.
//
// That is why `apply_outputs` was split out of `apply` in the first place; the
// header of zref_geom_warp.hpp says so: "this lets a differential test feed the
// HARDWARE's six words through the SAME application law and compare only the
// half under test, instead of re-deriving the whole operation and hoping the
// disagreement lands where you are looking." This file is that test.
//
// WHAT THIS BENCH PLAYS, AND WHAT IT DOES NOT. It plays the FIELD ADAPTER: it
// drives `f_ans_valid_i` / `f_warp_valid_i` and the six result words directly.
// It does NOT play the application arithmetic -- that is the device under test.
// The real `zhao_field_warp_adapter` against the real `zhao_field_host_v2` is a
// SEPARATE bench (geom_warp_field_pair_directed), because proving the
// application law and proving the transport are two different claims and W13
// insists they not be substituted for one another.
//
// THREE REFUSALS THE ORACLE CANNOT SEE, AND WHY THEY ARE CHECKED DIRECTLY.
// `apply_outputs` knows nothing about a profile mismatch, a Field transport
// fault, or the 64-bit seam narrowing, because none of the three is part of the
// APPLICATION law -- they are admission and transport faults this block owns.
// They are asserted against the contract's section 9 table by hand, and each one
// is SEEN TO MOVE ITS OWN COUNTER. A cause counter that never moved would be a
// detector reading zero, which is the claim to check hardest.
//
// ONE DELIBERATE DIVERGENCE FROM THE ORACLE, RECORDED RATHER THAN SMOOTHED.
// `apply_outputs` fills `displacement` before it tests the bound, so on a
// NEGATIVE-bound refusal it reports the returned displacement. The RTL refuses a
// negative bound BEFORE it offers anything to the Field, so no displacement
// exists to report and `poison_d*_o` reads zero. Refusing earlier is strictly
// better -- it spends no Field work on a command that was invalid on its face,
// which is 6.3's "validity condition of the COMMAND" -- and the test asserts the
// RTL's behaviour rather than pretending the two agree.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "verilated.h"

#include "Vzhao_geom_warp.h"
#include "zhao_sim.hpp"
#include "zref/zref_geom_warp.hpp"

namespace gw = zref::geom_warp;

using Dut = Vzhao_geom_warp;

// Cause codes, mirroring the RTL's localparams. Kept as a named enum so a
// miscompare prints a name rather than a digit.
enum Cause : int {
  C_NONE = 0,
  C_PROFILE = 1,
  C_NEG_BOUND = 2,
  C_BOUND = 3,
  C_NORMAL_WIDTH = 4,
  C_FIELD_FAULT = 5,
};

static const char* causeName(int c) {
  switch (c) {
    case C_NONE: return "none";
    case C_PROFILE: return "profile";
    case C_NEG_BOUND: return "negative-bound";
    case C_BOUND: return "bound-violation";
    case C_NORMAL_WIDTH: return "normal-width-fault";
    case C_FIELD_FAULT: return "field-fault";
  }
  return "?";
}

/** One vertex's worth of stimulus. */
struct Stim {
  // the post-skin vertex
  std::int32_t p[3] = {0, 0, 0};
  std::int64_t n[3] = {0, 0, 0};  // 64-bit, as GEOM.SKIN.NORM exports them
  bool n_degenerate = false;
  std::uint16_t src_id = 0;
  // the per-draw descriptor snapshot
  bool warp_en = true;
  std::uint8_t profile = 1;  // zfield::WARP
  std::int32_t bound[3] = {0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF};
  // what the played Field returns
  bool ans_valid = true;
  bool warp_valid = true;
  std::int32_t out6[6] = {0, 0, 0, 0, 0, 0};
  // backpressure knobs
  int stall_p = 0;
  int stall_n = 0;
};

/** What the device did with it. */
struct Outcome {
  bool published_p = false;
  bool published_n = false;
  std::int32_t p[3] = {0, 0, 0};
  std::int32_t n[3] = {0, 0, 0};
  bool degenerate = false;
  bool poisoned = false;
  int cause = C_NONE;
  std::int32_t poison_d[3] = {0, 0, 0};
  bool field_offered = false;  // did f_vtx_valid_o ever rise?
  int clocks = 0;
};

static void resetDut(Dut& d) {
  d.rst_n = 0;
  d.v_valid_i = 0;
  d.o_p_ready_i = 0;
  d.o_n_ready_i = 0;
  d.f_ans_valid_i = 0;
  d.f_warp_valid_i = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  zhao::tick(d);
}

/**
 * Drive one vertex all the way through and collect what came out.
 *
 * The offer is held until `v_ready_o`, the played answer is held until
 * `f_vtx_take_o`, and the two output halves are retired INDEPENDENTLY after
 * their own stall counts -- which is the only way to exercise the contract's
 * "accepted exactly once each" with the two consumers moving at different
 * speeds.
 */
static Outcome runOne(Dut& d, const Stim& s) {
  Outcome o{};

  d.v_px_i = s.p[0];
  d.v_py_i = s.p[1];
  d.v_pz_i = s.p[2];
  d.v_nx_i = s.n[0];
  d.v_ny_i = s.n[1];
  d.v_nz_i = s.n[2];
  d.v_n_degenerate_i = s.n_degenerate ? 1 : 0;
  d.v_src_id_i = s.src_id;
  d.v_attr_i[0] = 0; d.v_attr_i[1] = 0; d.v_attr_i[2] = 0; d.v_attr_i[3] = 0;

  d.d_warp_en_i = s.warp_en ? 1 : 0;
  d.d_slot_i = 0;
  d.d_slot_valid_i = 1;
  d.d_profile_i = s.profile;
  d.d_time_i = 0;
  d.d_par_i[0] = 0; d.d_par_i[1] = 0; d.d_par_i[2] = 0; d.d_par_i[3] = 0;
  d.d_bx_i = s.bound[0];
  d.d_by_i = s.bound[1];
  d.d_bz_i = s.bound[2];

  d.v_valid_i = 1;

  int p_wait = s.stall_p;
  int n_wait = s.stall_n;
  bool p_done = false, n_done = false;
  bool accepted = false;

  for (int guard = 0; guard < 400; ++guard) {
    // The played adapter answers whenever the block is asking.
    if (d.f_vtx_valid_o) {
      o.field_offered = true;
      d.f_ans_valid_i = s.ans_valid ? 1 : 0;
      d.f_warp_valid_i = s.warp_valid ? 1 : 0;
      d.f_dx_i = s.out6[0];
      d.f_dy_i = s.out6[1];
      d.f_dz_i = s.out6[2];
      d.f_onx_i = s.out6[3];
      d.f_ony_i = s.out6[4];
      d.f_onz_i = s.out6[5];
    } else {
      d.f_ans_valid_i = 0;
      d.f_warp_valid_i = 0;
    }

    // Independent consumed bits: each side counts down its OWN stall.
    d.o_p_ready_i = (!p_done && p_wait <= 0) ? 1 : 0;
    d.o_n_ready_i = (!n_done && n_wait <= 0) ? 1 : 0;

    if (d.o_p_valid_o && d.o_p_ready_i) {
      o.published_p = true;
      o.p[0] = d.o_px_o; o.p[1] = d.o_py_o; o.p[2] = d.o_pz_o;
      p_done = true;
    }
    if (d.o_n_valid_o && d.o_n_ready_i) {
      o.published_n = true;
      o.n[0] = d.o_nx_o; o.n[1] = d.o_ny_o; o.n[2] = d.o_nz_o;
      o.degenerate = d.o_n_degenerate_o != 0;
      n_done = true;
    }
    if (d.poison_valid_o) {
      o.poisoned = true;
      o.cause = d.poison_cause_o;
      o.poison_d[0] = d.poison_dx_o;
      o.poison_d[1] = d.poison_dy_o;
      o.poison_d[2] = d.poison_dz_o;
    }

    if (d.v_ready_o && d.v_valid_i) accepted = true;

    zhao::tick(d);
    ++o.clocks;
    if (accepted) d.v_valid_i = 0;
    if (p_wait > 0) --p_wait;
    if (n_wait > 0) --n_wait;

    if (o.poisoned) break;
    if (p_done && n_done) break;
  }

  d.v_valid_i = 0;
  d.o_p_ready_i = 0;
  d.o_n_ready_i = 0;
  d.f_ans_valid_i = 0;
  d.f_warp_valid_i = 0;
  zhao::tick(d);
  return o;
}

/** The oracle's verdict on the same stimulus, for the half it owns. */
static gw::Result oracleOf(const Stim& s) {
  gw::Inputs in{};
  in.position[0] = s.p[0];
  in.position[1] = s.p[1];
  in.position[2] = s.p[2];
  gw::Declaration decl{};
  decl.displacement_bound[0] = s.bound[0];
  decl.displacement_bound[1] = s.bound[1];
  decl.displacement_bound[2] = s.bound[2];
  return gw::apply_outputs(s.out6, in, decl, zfield::Status{});
}

/**
 * Compare a published outcome against the oracle, field by field. Used for
 * every case the APPLICATION law owns, so no case in this file carries a
 * hand-written expected position or normal.
 */
static void expectMatchesOracle(const char* what, const Stim& s, const Outcome& o) {
  const gw::Result r = oracleOf(s);
  if (r.poison()) {
    zhao::check(o.poisoned, "oracle poisons, so the RTL must poison", 1, o.poisoned ? 1 : 0);
    zhao::check(!o.published_p && !o.published_n,
                "W10: a poisoned vertex publishes on NEITHER port", 0,
                (o.published_p ? 1 : 0) + (o.published_n ? 1 : 0));
    return;
  }
  zhao::check(!o.poisoned, "oracle accepts, so the RTL must not poison", 0, o.poisoned ? 1 : 0);
  zhao::check(o.published_p && o.published_n, "both halves published", 2,
              (o.published_p ? 1 : 0) + (o.published_n ? 1 : 0));
  for (int k = 0; k < 3; ++k) {
    zhao::check(o.p[k] == r.position[k], what, static_cast<std::uint32_t>(r.position[k]),
                static_cast<std::uint32_t>(o.p[k]));
    zhao::check(o.n[k] == r.direction[k], what, static_cast<std::uint32_t>(r.direction[k]),
                static_cast<std::uint32_t>(o.n[k]));
  }
  zhao::check(o.degenerate == r.degenerate, "degeneracy agrees with the oracle",
              r.degenerate ? 1 : 0, o.degenerate ? 1 : 0);
}

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut top;
  resetDut(top);

  // ===================================================================== FT092
  // IDENTITY. d == 0 and n_out == n_in. Contract section 6: "An identity Warp
  // performs NO additional shift and lighting is exact." Exactness is the whole
  // claim -- an identity that moved a vertex by one LSB would be invisible in a
  // picture and fatal in a differential.
  {
    Stim s{};
    s.p[0] = 1234567; s.p[1] = -98765; s.p[2] = 42;
    s.n[0] = 1000; s.n[1] = -2000; s.n[2] = 3000;
    s.out6[0] = 0; s.out6[1] = 0; s.out6[2] = 0;
    s.out6[3] = 1000; s.out6[4] = -2000; s.out6[5] = 3000;
    s.src_id = 7;
    const std::uint32_t x0 = top.vertices_transformed_o;
    const std::uint32_t r0 = top.normal_reduced_o;
    const Outcome o = runOne(top, s);
    expectMatchesOracle("FT092 identity is exact", s, o);
    zhao::check(o.p[0] == s.p[0] && o.p[1] == s.p[1] && o.p[2] == s.p[2],
                "FT092 identity leaves the position bit-for-bit unchanged",
                static_cast<std::uint32_t>(s.p[0]), static_cast<std::uint32_t>(o.p[0]));
    zhao::check(o.n[0] == 1000 && o.n[1] == -2000 && o.n[2] == 3000,
                "FT092 identity leaves the normal bit-for-bit unchanged", 1000,
                static_cast<std::uint32_t>(o.n[0]));
    zhao::check(top.normal_reduced_o == r0,
                "FT092 identity performs NO range-reduction shift", r0, top.normal_reduced_o);
    zhao::check(top.vertices_transformed_o == x0 + 1,
                "FT092 counts as a successful ENABLED application", x0 + 1,
                top.vertices_transformed_o);
  }

  // ===================================================================== FT093
  // NON-IDENTITY. A plain translation, and the normal genuinely replaced.
  {
    Stim s{};
    s.p[0] = 1000; s.p[1] = 2000; s.p[2] = 3000;
    s.n[0] = 5; s.n[1] = 6; s.n[2] = 7;
    s.out6[0] = 111; s.out6[1] = -222; s.out6[2] = 333;
    s.out6[3] = -50; s.out6[4] = 60; s.out6[5] = -70;
    const Outcome o = runOne(top, s);
    expectMatchesOracle("FT093 non-identity translation", s, o);
    zhao::check(o.p[0] == 1111 && o.p[1] == 1778 && o.p[2] == 3333,
                "FT093 position is Pin + d componentwise", 1111,
                static_cast<std::uint32_t>(o.p[0]));
    zhao::check(o.n[0] == -50 && o.n[1] == 60 && o.n[2] == -70,
                "FT093 the normal is REPLACED, not blended with the input",
                static_cast<std::uint32_t>(-50), static_cast<std::uint32_t>(o.n[0]));
  }

  // ===================================================================== FT094
  // THE SIGNED RAILS. The application add saturates, and its saturation is
  // counted SEPARATELY from the Field program's [5.3].
  {
    Stim s{};
    s.p[0] = 0x7FFFFFFF; s.p[1] = static_cast<std::int32_t>(0x80000000); s.p[2] = 0;
    s.n[0] = 1; s.n[1] = 1; s.n[2] = 1;
    s.out6[0] = 1000; s.out6[1] = -1000; s.out6[2] = 0;
    s.out6[3] = 1; s.out6[4] = 1; s.out6[5] = 1;
    const std::uint32_t a0 = top.app_saturations_o;
    const Outcome o = runOne(top, s);
    expectMatchesOracle("FT094 saturating rails", s, o);
    zhao::check(o.p[0] == 0x7FFFFFFF, "FT094 positive rail CLAMPS, never wraps", 0x7FFFFFFFu,
                static_cast<std::uint32_t>(o.p[0]));
    zhao::check(o.p[1] == static_cast<std::int32_t>(0x80000000),
                "FT094 negative rail CLAMPS, never wraps", 0x80000000u,
                static_cast<std::uint32_t>(o.p[1]));
    zhao::check(top.app_saturations_o == a0 + 1,
                "FT094 APPLICATION saturation has its own counter and it MOVED", a0 + 1,
                top.app_saturations_o);
  }

  // ===================================================================== FT095
  // THE BOUND, AND ITS EXACT BOUNDARY. 5.6 says `|d[k]| > bound[k]` poisons --
  // strictly greater. The equal case is LEGAL and both are tested, because a
  // `>=` written where `>` belongs passes every test that only ever tries a
  // clear violation.
  {
    Stim eq{};
    eq.p[0] = 0; eq.p[1] = 0; eq.p[2] = 0;
    eq.n[0] = 1; eq.n[1] = 1; eq.n[2] = 1;
    eq.bound[0] = 500; eq.bound[1] = 500; eq.bound[2] = 500;
    eq.out6[0] = 500; eq.out6[1] = -500; eq.out6[2] = 0;
    eq.out6[3] = 1; eq.out6[4] = 1; eq.out6[5] = 1;
    const Outcome o = runOne(top, eq);
    expectMatchesOracle("FT095 |d| == bound is LEGAL", eq, o);
    zhao::check(!o.poisoned, "FT095 the `>` boundary: equality does NOT poison", 0,
                o.poisoned ? 1 : 0);

    Stim over = eq;
    over.out6[0] = 501;
    const std::uint32_t b0 = top.bound_violations_o;
    const Outcome o2 = runOne(top, over);
    zhao::check(o2.poisoned && o2.cause == C_BOUND, "FT095 |d| > bound POISONS", C_BOUND,
                o2.cause);
    zhao::check(!o2.published_p && !o2.published_n,
                "FT095 W10: the violating vertex publishes on neither port", 0,
                (o2.published_p ? 1 : 0) + (o2.published_n ? 1 : 0));
    zhao::check(o2.poison_d[0] == 501,
                "FT095 5.6: the RETURNED displacement is PRESERVED, not clamped -- the "
                "diagnosis is made from it",
                501, static_cast<std::uint32_t>(o2.poison_d[0]));
    zhao::check(top.bound_violations_o == b0 + 1,
                "FT095 the bound-violation counter MOVED", b0 + 1, top.bound_violations_o);
    expectMatchesOracle("FT095 the oracle agrees the bound is violated", over, o2);
  }

  // ===================================================================== FT096
  // THE NORMAL, AND THE CASE THAT BREAKS THE TEMPTING ONE-LINER.
  //
  // -(2^31 - 1) has magnitude below 2^31, so "shifts = (max>=2^31)?2:1" says ONE
  // shift. An arithmetic right shift rounds toward negative infinity, so
  // -(2^31-1) >> 1 is exactly -2^30, still AT the ceiling, and the real loop
  // shifts AGAIN. The RTL header explains this; here it is measured.
  {
    struct { std::int32_t v; const char* what; } cases[] = {
        {1 << 29, "below the ceiling: no shift"},
        {1 << 30, "exactly at the ceiling: one shift"},
        {-(1 << 30), "negative at the ceiling"},
        {0x7FFFFFFF, "INT32_MAX"},
        {static_cast<std::int32_t>(0x80000000), "INT32_MIN: two shifts, the widened-abs case"},
        {-2147483647, "-(2^31 - 1): THE CASE THE ONE-LINER GETS WRONG"},
    };
    for (const auto& c : cases) {
      Stim s{};
      s.p[0] = 0; s.p[1] = 0; s.p[2] = 0;
      s.n[0] = 1; s.n[1] = 1; s.n[2] = 1;
      s.out6[0] = 0; s.out6[1] = 0; s.out6[2] = 0;
      s.out6[3] = c.v; s.out6[4] = 0; s.out6[5] = 0;
      const Outcome o = runOne(top, s);
      expectMatchesOracle(c.what, s, o);
      // And independently of the oracle: the published normal is always below
      // the ceiling. This is the PROPERTY, asserted directly, so a matching pair
      // of wrong answers could not both pass.
      const std::int64_t mag =
          o.n[0] < 0 ? -static_cast<std::int64_t>(o.n[0]) : static_cast<std::int64_t>(o.n[0]);
      zhao::check(mag < (std::int64_t{1} << 30),
                  "FT096 the reduced normal is ALWAYS below 2^30", 1,
                  mag < (std::int64_t{1} << 30) ? 1 : 0);
    }
  }

  // ---- the degenerate normal (5.4: zero iff ALL THREE words are zero) -------
  {
    Stim s{};
    s.p[0] = 5; s.p[1] = 5; s.p[2] = 5;
    s.n[0] = 9; s.n[1] = 9; s.n[2] = 9;
    s.out6[3] = 0; s.out6[4] = 0; s.out6[5] = 0;
    const std::uint32_t g0 = top.degenerate_o;
    const Outcome o = runOne(top, s);
    expectMatchesOracle("all-zero normal is degenerate", s, o);
    zhao::check(o.degenerate, "the all-three-zero rule marks the vertex degenerate", 1,
                o.degenerate ? 1 : 0);
    zhao::check(top.degenerate_o == g0 + 1, "the degenerate counter MOVED", g0 + 1,
                top.degenerate_o);

    // ONE nonzero word is NOT degenerate -- the discriminating case. A rule
    // written as "any word is zero" passes the test above and fails this one.
    Stim one = s;
    one.out6[4] = 1;
    const Outcome o2 = runOne(top, one);
    zhao::check(!o2.degenerate,
                "ONE nonzero word is NOT degenerate -- the case that separates "
                "'all three zero' from 'any zero'",
                0, o2.degenerate ? 1 : 0);
    expectMatchesOracle("one nonzero word", one, o2);
  }

  // ---- W09: THE ORDINARY PATH PERFORMS ZERO WARP LOOKUPS --------------------
  // The directive does not merely ask that a bypass produce the right answer; it
  // asks that it do NO Warp work. `field_offered` watches `f_vtx_valid_o` for
  // the whole transaction, so "it never asked" is MEASURED, not argued.
  {
    Stim s{};
    s.warp_en = false;
    s.p[0] = 77; s.p[1] = -88; s.p[2] = 99;
    s.n[0] = 11; s.n[1] = 22; s.n[2] = 33;
    s.out6[0] = 999999;  // would be applied if the bypass leaked
    const std::uint32_t b0 = top.bypassed_o;
    const std::uint32_t x0 = top.vertices_transformed_o;
    const Outcome o = runOne(top, s);
    zhao::check(!o.field_offered,
                "W09: DrawForm performs ZERO Warp lookups -- f_vtx_valid_o never rose", 0,
                o.field_offered ? 1 : 0);
    zhao::check(o.p[0] == 77 && o.p[1] == -88 && o.p[2] == 99,
                "W09 the bypassed position is preserved exactly", 77,
                static_cast<std::uint32_t>(o.p[0]));
    zhao::check(o.n[0] == 11 && o.n[1] == 22 && o.n[2] == 33,
                "W09 the bypassed normal is preserved exactly, with NO extra normalization", 11,
                static_cast<std::uint32_t>(o.n[0]));
    zhao::check(top.bypassed_o == b0 + 1, "W09 the BYPASS counter moved", b0 + 1,
                top.bypassed_o);
    zhao::check(top.vertices_transformed_o == x0,
                "W09 and `vertices_transformed` did NOT -- section 20 pins it to "
                "successful ENABLED applications, not bypasses",
                x0, top.vertices_transformed_o);
  }

  // ---- the three refusals the oracle cannot see, each SEEN TO FIRE ----------
  {
    // (a) profile mismatch -- an Earth program is not a Warp program.
    Stim s{};
    s.n[0] = 1; s.n[1] = 1; s.n[2] = 1;
    s.profile = 2;  // not WARP_PROFILE_ID
    const std::uint32_t p0 = top.profile_mismatches_o;
    const Outcome o = runOne(top, s);
    zhao::check(o.poisoned && o.cause == C_PROFILE, "profile mismatch poisons", C_PROFILE,
                o.cause);
    zhao::check(!o.field_offered,
                "a profile mismatch is refused BEFORE any Field work is offered", 0,
                o.field_offered ? 1 : 0);
    zhao::check(top.profile_mismatches_o == p0 + 1, "the profile counter MOVED", p0 + 1,
                top.profile_mismatches_o);
  }
  {
    // (b) a negative bound -- 6.3, a validity condition of the COMMAND.
    Stim s{};
    s.n[0] = 1; s.n[1] = 1; s.n[2] = 1;
    s.bound[1] = -1;
    const std::uint32_t n0 = top.negative_bounds_o;
    const Outcome o = runOne(top, s);
    zhao::check(o.poisoned && o.cause == C_NEG_BOUND, "a negative bound poisons", C_NEG_BOUND,
                o.cause);
    zhao::check(!o.field_offered,
                "a negative bound is refused before any Field work is offered", 0,
                o.field_offered ? 1 : 0);
    zhao::check(top.negative_bounds_o == n0 + 1, "the negative-bound counter MOVED", n0 + 1,
                top.negative_bounds_o);
    // The oracle refuses it too, for the same reason, on the same stimulus.
    const gw::Result r = oracleOf(s);
    zhao::check(r.refusal == gw::Refusal::kNegativeBound,
                "and the ORACLE refuses the identical descriptor", 1,
                r.refusal == gw::Refusal::kNegativeBound ? 1 : 0);
  }
  {
    // (c) a Field answer that is not a Field result. W10: this must NOT become
    // an identity deformation and must NOT become a zero. The data cannot tell
    // you -- a legitimate identity program returns all zeroes too -- which is
    // exactly why `warp_valid` is a separate wire from `ans_valid`.
    Stim s{};
    s.n[0] = 1; s.n[1] = 1; s.n[2] = 1;
    s.warp_valid = false;
    const std::uint32_t f0 = top.field_faults_o;
    const Outcome o = runOne(top, s);
    zhao::check(o.poisoned && o.cause == C_FIELD_FAULT, "a failed evaluation poisons",
                C_FIELD_FAULT, o.cause);
    zhao::check(!o.published_p && !o.published_n,
                "W10: a failed evaluation does NOT become a published zero result", 0,
                (o.published_p ? 1 : 0) + (o.published_n ? 1 : 0));
    zhao::check(top.field_faults_o == f0 + 1, "the field-fault counter MOVED", f0 + 1,
                top.field_faults_o);
  }
  {
    // (d) THE 64-BIT SEAM. GEOM.SKIN.NORM's `n_x_o` is signed [63:0]. A value
    // that does not fit signed 32 is a SEAM FAULT, not permission to truncate:
    // truncation would manufacture a plausible small number from an impossible
    // large one and nothing downstream could tell.
    Stim s{};
    s.n[0] = std::int64_t{1} << 40;  // does not fit s32
    s.n[1] = 0;
    s.n[2] = 0;
    const std::uint32_t w0 = top.normal_width_faults_o;
    const Outcome o = runOne(top, s);
    zhao::check(o.poisoned && o.cause == C_NORMAL_WIDTH, "an out-of-range normal poisons",
                C_NORMAL_WIDTH, o.cause);
    zhao::check(!o.field_offered,
                "the seam fault is caught on INTAKE, before anything is submitted", 0,
                o.field_offered ? 1 : 0);
    zhao::check(top.normal_width_faults_o == w0 + 1, "the normal-width counter MOVED", w0 + 1,
                top.normal_width_faults_o);

    // The NEGATIVE CONTROL for the same check: the largest value that DOES fit
    // must pass. A width check written as "any high bit set" would fail this.
    Stim ok = s;
    ok.n[0] = static_cast<std::int64_t>(static_cast<std::int32_t>(0x80000000));
    ok.out6[3] = 1;
    const std::uint32_t w1 = top.normal_width_faults_o;
    const Outcome o2 = runOne(top, ok);
    zhao::check(!o2.poisoned,
                "NEGATIVE CONTROL: a sign-extended INT32_MIN DOES fit and is accepted", 0,
                o2.poisoned ? 1 : 0);
    zhao::check(top.normal_width_faults_o == w1,
                "and the width counter did NOT move on the legal value", w1,
                top.normal_width_faults_o);
  }

  // ---- the fork: accepted EXACTLY ONCE each, at different speeds ------------
  // Contract section 4 and directive 11.6. A fork that ANDs the two readys while
  // leaving both valids asserted previously DUPLICATED accepted work elsewhere
  // in this console. Two independent accept counters can see that; one shared
  // counter could not.
  {
    const std::uint32_t pa0 = top.p_accepts_o;
    const std::uint32_t na0 = top.n_accepts_o;
    int published = 0;
    for (int i = 0; i < 8; ++i) {
      Stim s{};
      s.p[0] = i * 100;
      s.n[0] = 1; s.n[1] = 1; s.n[2] = 1;
      s.out6[0] = i;
      s.out6[3] = 1; s.out6[4] = 2; s.out6[5] = 3;
      s.stall_p = (i % 3);      // the two consumers move at DIFFERENT speeds
      s.stall_n = 2 - (i % 3);
      const Outcome o = runOne(top, s);
      zhao::check(o.published_p && o.published_n, "both halves retired under skewed stalls", 2,
                  (o.published_p ? 1 : 0) + (o.published_n ? 1 : 0));
      expectMatchesOracle("value survives skewed backpressure", s, o);
      ++published;
    }
    zhao::check(top.p_accepts_o == pa0 + published, "position accepted exactly once per vertex",
                pa0 + published, top.p_accepts_o);
    zhao::check(top.n_accepts_o == na0 + published, "normal accepted exactly once per vertex",
                na0 + published, top.n_accepts_o);
    zhao::check(top.p_accepts_o == top.n_accepts_o,
                "contract section 10 conservation: position accepts == normal accepts",
                top.n_accepts_o, top.p_accepts_o);
  }

  // ---- a randomized differential over the whole application law -------------
  // Hostile values by construction: the rails, the ceiling and their neighbours
  // are sampled far more often than a uniform draw would reach them.
  {
    static const std::int32_t kInteresting[] = {
        0, 1, -1, 2, -2,
        (1 << 29), (1 << 30), -(1 << 30), (1 << 30) + 1, -((1 << 30) + 1),
        0x7FFFFFFF, static_cast<std::int32_t>(0x80000000), 2147483646, -2147483647,
        1000, -1000, 65536, -65536,
    };
    const int kN = static_cast<int>(sizeof(kInteresting) / sizeof(kInteresting[0]));
    std::uint32_t lcg = 0x5EED1234u;
    auto pick = [&]() -> std::int32_t {
      lcg = lcg * 1664525u + 1013904223u;
      return kInteresting[(lcg >> 16) % kN];
    };
    int compared = 0, poisoned = 0, saturated = 0;
    for (int i = 0; i < 600; ++i) {
      Stim s{};
      s.p[0] = pick(); s.p[1] = pick(); s.p[2] = pick();
      s.n[0] = 1; s.n[1] = 1; s.n[2] = 1;
      for (int k = 0; k < 6; ++k) s.out6[k] = pick();
      // A bound that is sometimes generous and sometimes tight, always legal.
      const std::int32_t b = pick();
      const std::int32_t nb = (b < 0) ? -(b + 1) : b;  // nonnegative, no INT32_MIN overflow
      s.bound[0] = nb; s.bound[1] = nb; s.bound[2] = nb;
      const gw::Result r = oracleOf(s);
      const Outcome o = runOne(top, s);
      expectMatchesOracle("randomized differential", s, o);
      ++compared;
      if (r.poison()) ++poisoned;
      if (r.app_saturated) ++saturated;
    }
    std::printf("  randomized: %d compared, %d poisoned by bound, %d application-saturated\n",
                compared, poisoned, saturated);
    // A differential that never reached a refusal or a rail would be reporting
    // agreement about the easy half only. These are coverage assertions on the
    // STIMULUS, not on the device.
    zhao::check(poisoned > 20, "the randomized sweep actually REACHED bound violations", 1,
                poisoned > 20 ? 1 : 0);
    zhao::check(saturated > 20, "the randomized sweep actually REACHED the saturating rails", 1,
                saturated > 20 ? 1 : 0);
  }

  std::printf(
      "  counters: transformed=%u bypassed=%u appsat=%u reduced=%u degenerate=%u\n"
      "            bound=%u negbound=%u nwidth=%u profile=%u fieldfault=%u\n"
      "            p_accepts=%u n_accepts=%u\n",
      top.vertices_transformed_o, top.bypassed_o, top.app_saturations_o, top.normal_reduced_o,
      top.degenerate_o, top.bound_violations_o, top.negative_bounds_o,
      top.normal_width_faults_o, top.profile_mismatches_o, top.field_faults_o, top.p_accepts_o,
      top.n_accepts_o);

  // EVERY cause counter must have moved. A cause that never fired would mean
  // this file contains a refusal path nothing exercises -- a detector reading
  // zero, quoted as coverage.
  zhao::check(top.bound_violations_o > 0 && top.negative_bounds_o > 0 &&
                  top.normal_width_faults_o > 0 && top.profile_mismatches_o > 0 &&
                  top.field_faults_o > 0 && top.app_saturations_o > 0 &&
                  top.normal_reduced_o > 0 && top.degenerate_o > 0 && top.bypassed_o > 0,
              "EVERY counter this block declares was SEEN TO FIRE", 1, 1);

  return zhao::report_and_exit("geom_warp_rtl_directed");
}
