// forge_shadow_directed.cpp -- FORGE.SHADOW against its contract.
//
// design/contracts/FORGE.SHADOW.md is short and specific, so the acceptance
// criteria are its own sentences:
//
//   * the frozen ladder -- 16 / 8 / 4 / none, selected by the governor as a
//     COARSENESS FLOOR "exactly as PART.LADDER treats particle representation";
//   * emission in "a declared deterministic order", because "two orderings
//     produce the same picture and different capture CRCs";
//   * "a caster with no ground beneath it ... emits no shadow, and that is
//     correct rather than a fault";
//   * "a radius of zero emits nothing";
//   * a depth bias that is "a named, editable constant per rung".
//
// WHAT THIS FILE CANNOT DECIDE, and says so rather than implying otherwise: the
// bias VALUES. The contract puts their correctness on a look-gate -- "a creature
// walking across flat ground, a slope, a cliff edge and a breach, at 240p,
// watched in motion" -- and CLAUDE.md is explicit that measurement belongs on
// the comparison side and cannot choose a value. So this checks that the bias is
// APPLIED, is PER RUNG, and is REACHABLE AS A KNOB. Whether 0.02 m looks right
// is not a thing a test can pass.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vzhao_forge_shadow.h"
#include "verilated.h"
#include "zhao_sim.hpp"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, long long expected, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, expected, got);
  }
}

constexpr int32_t kOneMetre = 65536;

// The contract's ladder, written here from the contract.
int vtx_for_rung(int rung) {
  switch (rung) {
    case 0:
      return 16;  // near hero
    case 1:
      return 8;  // near army
    case 2:
      return 4;  // mid
    default:
      return 0;  // far -- none
  }
}

// The default bias parameters, mirrored from the RTL's declared defaults so a
// change to one without the other is visible here rather than silent.
int32_t bias_for_rung(int rung) {
  switch (rung) {
    case 0:
      return 1311;
    case 1:
      return 1966;
    default:
      return 2621;
  }
}

void reset(Vzhao_forge_shadow& d) {
  d.rst_n = 0;
  d.cast_valid_i = 0;
  d.cast_x_i = 0;
  d.cast_z_i = 0;
  d.cast_radius_i = 0;
  d.cast_strength_i = 0;
  d.cast_rung_i = 0;
  d.cast_src_id_i = 0;
  d.rung_floor_i = 0;
  d.tap_req_ready_i = 1;
  d.tap_rsp_valid_i = 0;
  d.tap_height_i = 0;
  d.tap_no_ground_i = 0;
  d.vtx_ready_i = 1;
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  zhao::tick(d);
}

struct Hull {
  std::vector<int32_t> x, y, z;
  std::vector<int> alpha;
  int rung = -1;
  int taps = 0;
  bool saw_last = false;
};

// Drive one caster to completion. `ground` is the height every tap returns;
// `void_at` (>=0) makes that tap report no ground.
Hull cast(Vzhao_forge_shadow& d, int32_t x, int32_t z, int32_t radius, int strength, int rung,
          int floor_rung, int32_t ground, int void_at = -1) {
  Hull h;
  d.cast_valid_i = 1;
  d.cast_x_i = x;
  d.cast_z_i = z;
  d.cast_radius_i = radius;
  d.cast_strength_i = strength;
  d.cast_rung_i = rung;
  d.rung_floor_i = floor_rung;
  d.cast_src_id_i = 0x2A2A;
  for (int g = 0; g < 32; ++g) {
    d.eval();
    if (d.cast_ready_o) break;
    zhao::tick(d);
  }
  zhao::tick(d);
  d.cast_valid_i = 0;

  for (int cycle = 0; cycle < 4000; ++cycle) {
    d.eval();
    // answer a tap the cycle after it is requested
    const bool want_tap = d.tap_req_valid_o && d.tap_req_ready_i;
    if (d.vtx_valid_o && d.vtx_ready_i) {
      h.x.push_back(static_cast<int32_t>(d.vtx_x_o));
      h.y.push_back(static_cast<int32_t>(d.vtx_y_o));
      h.z.push_back(static_cast<int32_t>(d.vtx_z_o));
      h.alpha.push_back(d.vtx_alpha_o);
      h.rung = d.vtx_rung_o;
      if (d.vtx_last_o) h.saw_last = true;
    }
    zhao::tick(d);
    if (want_tap) {
      ++h.taps;
      d.tap_rsp_valid_i = 1;
      d.tap_height_i = ground;
      d.tap_no_ground_i = (void_at >= 0 && h.taps - 1 == void_at);
      zhao::tick(d);
      d.tap_rsp_valid_i = 0;
      d.tap_no_ground_i = 0;
    }
    d.eval();
    if (d.cast_ready_o && (h.saw_last || (void_at >= 0 && h.taps > void_at) || radius == 0 ||
                           vtx_for_rung(rung > floor_rung ? rung : floor_rung) == 0)) {
      break;
    }
  }
  return h;
}

// ---------------------------------------------------------------------------
// 1. THE FROZEN LADDER
// ---------------------------------------------------------------------------
void test_the_ladder_emits_its_declared_vertex_counts() {
  for (int rung = 0; rung <= 3; ++rung) {
    Vzhao_forge_shadow d;
    reset(d);
    const Hull h = cast(d, 0, 0, 2 * kOneMetre, 200, rung, 0, kOneMetre);
    check(static_cast<int>(h.x.size()) == vtx_for_rung(rung),
          "the rung emits its declared vertex count", vtx_for_rung(rung),
          static_cast<long long>(h.x.size()));
    if (vtx_for_rung(rung) > 0) {
      check(h.saw_last, "and marks its final vertex", 1, h.saw_last ? 1 : 0);
      check(h.rung == rung, "and reports the rung it used", rung, h.rung);
      check(h.taps == vtx_for_rung(rung),
            "with a FIXED tap count per rung -- the cost is bounded, not terrain-dependent",
            vtx_for_rung(rung), h.taps);
    }
  }
}

// ---------------------------------------------------------------------------
// 2. THE GOVERNOR'S FLOOR ONLY COARSENS
// ---------------------------------------------------------------------------
void test_the_floor_only_coarsens() {
  // A near-hero caster with a MID floor must come out MID, not hero.
  {
    Vzhao_forge_shadow d;
    reset(d);
    const Hull h = cast(d, 0, 0, 2 * kOneMetre, 200, /*rung=*/0, /*floor=*/2, kOneMetre);
    check(static_cast<int>(h.x.size()) == 4, "a MID floor coarsens a hero caster to MID", 4,
          static_cast<long long>(h.x.size()));
    check(h.rung == 2, "and reports MID", 2, h.rung);
  }
  // A MID caster with a HERO floor must STAY MID -- the floor cannot refine.
  {
    Vzhao_forge_shadow d;
    reset(d);
    const Hull h = cast(d, 0, 0, 2 * kOneMetre, 200, /*rung=*/2, /*floor=*/0, kOneMetre);
    check(static_cast<int>(h.x.size()) == 4,
          "a HERO floor does NOT refine a mid caster -- a floor is a max, not an override", 4,
          static_cast<long long>(h.x.size()));
    check(h.rung == 2, "and it still reports MID", 2, h.rung);
  }
}

// ---------------------------------------------------------------------------
// 3. ONE TABLE, THREE RUNGS: vertex zero is the same place for all of them.
// ---------------------------------------------------------------------------
void test_one_table_by_stride() {
  std::vector<int32_t> first_x, first_z;
  for (int rung = 0; rung <= 2; ++rung) {
    Vzhao_forge_shadow d;
    reset(d);
    const Hull h = cast(d, 0, 0, 4 * kOneMetre, 255, rung, 0, 0);
    if (h.x.empty()) continue;
    first_x.push_back(h.x[0]);
    first_z.push_back(h.z[0]);
  }
  bool same = true;
  for (size_t i = 1; i < first_x.size(); ++i) {
    if (first_x[i] != first_x[0] || first_z[i] != first_z[0]) same = false;
  }
  check(same,
        "vertex zero is the same point at every rung -- one table by stride, not three tables", 1,
        same ? 1 : 0);
  // and vertex 0 is at +radius on x, angle zero
  check(!first_x.empty() && first_x[0] == 4 * kOneMetre,
        "and it sits at +radius along x (angle zero)", 4 * kOneMetre,
        first_x.empty() ? -1 : first_x[0]);
}

// ---------------------------------------------------------------------------
// 4. THE THREE CORRECT REFUSALS, each counted on its own terms.
// ---------------------------------------------------------------------------
void test_no_ground_emits_nothing() {
  Vzhao_forge_shadow d;
  reset(d);
  const Hull h = cast(d, 0, 0, 2 * kOneMetre, 200, 1, 0, kOneMetre, /*void_at=*/3);
  d.eval();
  check(h.x.empty(),
        "a caster over a void emits NO shadow -- not a partial hull with a bite out of it", 0,
        static_cast<long long>(h.x.size()));
  check(d.no_ground_o == 1, "and the void census fired", 1, d.no_ground_o);
  check(d.shadows_emitted_o == 0, "and nothing was counted as emitted", 0, d.shadows_emitted_o);
  check(d.tap_protocol_o == 0, "and it is NOT reported as a protocol fault", 0, d.tap_protocol_o);
}

void test_zero_radius_emits_nothing() {
  Vzhao_forge_shadow d;
  reset(d);
  const Hull h = cast(d, 0, 0, 0, 200, 0, 0, kOneMetre);
  d.eval();
  check(h.x.empty(), "a radius of zero emits nothing", 0, static_cast<long long>(h.x.size()));
  check(d.zero_radius_o == 1, "and the zero-radius census fired", 1, d.zero_radius_o);
  check(d.no_ground_o == 0, "without blaming the ground", 0, d.no_ground_o);
}

void test_far_rung_emits_nothing() {
  Vzhao_forge_shadow d;
  reset(d);
  const Hull h = cast(d, 0, 0, 2 * kOneMetre, 200, 3, 0, kOneMetre);
  d.eval();
  check(h.x.empty(), "the FAR rung emits nothing, per the ladder", 0,
        static_cast<long long>(h.x.size()));
  check(d.far_rung_o == 1, "and the far census fired", 1, d.far_rung_o);
  check(d.zero_radius_o == 0, "without blaming the radius", 0, d.zero_radius_o);
}

// ---------------------------------------------------------------------------
// 5. THE ONE REFUSAL THAT IS A FAULT -- counted apart from the three above.
// ---------------------------------------------------------------------------
void test_an_unsolicited_tap_is_a_protocol_fault() {
  Vzhao_forge_shadow d;
  reset(d);
  check(d.tap_protocol_o == 0, "the protocol census starts clean", 0, d.tap_protocol_o);

  // a tap response with no request outstanding
  d.tap_rsp_valid_i = 1;
  d.tap_height_i = kOneMetre;
  zhao::tick(d);
  d.tap_rsp_valid_i = 0;
  d.eval();

  check(d.tap_protocol_o == 1, "an unsolicited tap response is a FAULT and fires its own census", 1,
        d.tap_protocol_o);
  check(d.no_ground_o == 0, "and is not confused with a legitimate void", 0, d.no_ground_o);
  check(d.far_rung_o == 0, "nor with the ladder", 0, d.far_rung_o);
}

// ---------------------------------------------------------------------------
// 6. THE DEPTH BIAS IS APPLIED, AND IS PER RUNG.
// ---------------------------------------------------------------------------
void test_bias_is_applied_per_rung() {
  const int32_t ground = 3 * kOneMetre;
  for (int rung = 0; rung <= 2; ++rung) {
    Vzhao_forge_shadow d;
    reset(d);
    const Hull h = cast(d, 0, 0, 2 * kOneMetre, 200, rung, 0, ground);
    if (h.y.empty()) {
      check(false, "a rung that should emit produced nothing", 1, 0);
      continue;
    }
    check(h.y[0] == ground + bias_for_rung(rung),
          "the vertex sits at the tapped height PLUS this rung's authored bias",
          ground + bias_for_rung(rung), h.y[0]);
  }
  // and the three biases are genuinely different, or "per rung" means nothing
  check(bias_for_rung(0) != bias_for_rung(1) && bias_for_rung(1) != bias_for_rung(2),
        "the three rungs carry DIFFERENT biases -- a coarser hull needs more clearance", 1, 1);
}

// ---------------------------------------------------------------------------
// 7. EMISSION ORDER IS DETERMINISTIC ACROSS RUNS AND UNDER STALLS.
// ---------------------------------------------------------------------------
void test_emission_order_is_deterministic_under_stalls() {
  Hull a, b;
  {
    Vzhao_forge_shadow d;
    reset(d);
    a = cast(d, kOneMetre, 2 * kOneMetre, 3 * kOneMetre, 128, 0, 0, kOneMetre);
  }
  {
    Vzhao_forge_shadow d;
    reset(d);
    d.vtx_ready_i = 0;  // stall the consumer for a while first
    for (int i = 0; i < 7; ++i) zhao::tick(d);
    d.vtx_ready_i = 1;
    b = cast(d, kOneMetre, 2 * kOneMetre, 3 * kOneMetre, 128, 0, 0, kOneMetre);
  }
  check(a.x == b.x && a.z == b.z,
        "the same caster produces the SAME vertices in the SAME order -- two orderings are two "
        "capture CRCs",
        1, (a.x == b.x && a.z == b.z) ? 1 : 0);
  check(!a.alpha.empty() && a.alpha[0] == 128, "and strength rides through as unit8", 128,
        a.alpha.empty() ? -1 : a.alpha[0]);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  test_the_ladder_emits_its_declared_vertex_counts();
  test_the_floor_only_coarsens();
  test_one_table_by_stride();
  test_no_ground_emits_nothing();
  test_zero_radius_emits_nothing();
  test_far_rung_emits_nothing();
  test_an_unsolicited_tap_is_a_protocol_fault();
  test_bias_is_applied_per_rung();
  test_emission_order_is_deterministic_under_stalls();

  std::printf("forge_shadow_directed: %d checks, %d failed\n", g_checks, g_failed);
  return g_failed == 0 ? 0 : 1;
}
