// nav_service_directed.cpp — the acceptance suite for SW.CPUCOLL's navigation
// query, against the PRODUCTION API (`zref::nav::Service`) and real terrain.
//
// The owner's five acceptance items (reports/OWNER-DECISION-20260926-I34-NAV.md
// §5) are sections 1..5 below, in his order. Nothing here injects a final cost
// into a fake consumer: every number comes out of `Service::query` /
// `Service::query_cell` after a real `add_field` of a real `ZhCmdTerrainField`
// at a real tick.
//
// TWO KINDS OF PROGRAM ARE USED AND THE DIFFERENCE MATTERS.
//
//   * `crater_ring` is a REAL SHIPPED SPELL — compiler/tests/generated/
//     crater_ring.hpp, emitted from spells/upheaval.form by the nanquan
//     compiler and committed with its program hash. Its Form source writes
//     `nav_cost = height/4` (nanquan/src/field_ir/crater_ring.ts:91). It is
//     decoded here through `zfield::decode`, so the bytes are validated exactly
//     as a cartridge's would be. It proves the whole chain end to end and it is
//     the evidence for acceptance item 2.
//   * The hand-built programs below are STIMULUS: a constant nav delta, a
//     negative one, a saturating one, an age-varying one, and one with only
//     three out-lanes. They exist to drive a SPECIFIC value into the ratified
//     reduction so the law can be checked at a known point. They are not a
//     second implementation of anything -- `zfield::interpret` executes them,
//     and `zref::fieldir::compose_nav` reduces their output.
//
// PROVE THE INSTRUMENT. Every "unchanged" assertion in this file has a
// companion that CHANGES on the same fixture with one thing moved, because an
// equality that holds for the wrong reason is this campaign's most expensive
// habit. They are marked `[control]`.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <queue>
#include <vector>

#include "crater_ring.hpp"  // compiler/tests/generated: a REAL committed spell

#include "zfield/zfield.hpp"
#include "zref/zref_fieldir.hpp"
#include "zref/zref_nav.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_terrain.hpp"
#include "zref/zref_terrain_nav.hpp"

namespace {

using zhao_abi::ZhCmdTerrainField;
using zhao_abi::ZhTransform2fx;

int failures = 0;
int checks = 0;

void check(bool ok, const char* what) {
  ++checks;
  if (ok) return;
  std::printf("  FAIL: %s\n", what);
  ++failures;
}

void check_eq(int64_t a, int64_t b, const char* what) {
  ++checks;
  if (a == b) return;
  std::printf("  FAIL: %s (expected %lld, got %lld)\n", what, static_cast<long long>(b),
              static_cast<long long>(a));
  ++failures;
}

constexpr int32_t kOne = 1 << 16;  // Q16.16 one
constexpr int kLat = 33;           // 33x33 vertices, 32x32 cells
constexpr int kCells = kLat - 1;
constexpr int32_t kPitchM = 2;                    // canonical battlefield pitch, terrain_rules 1.3
constexpr int32_t kSpan = kCells * kPitchM * kOne;  // 64 m envelope, fx16

// ---------------------------------------------------------------- terrain ---

/**
 * The fixture terrain, authored by hand so every acceptance answer has a
 * hand-checkable cause:
 *
 *   * level ground everywhere except
 *   * a STEEP RIDGE at lattice columns 24..26 (6 m per 2 m step = slope 3,
 *     n.y ~ 0.316, under the default 0.6 limit -> impassable), and
 *   * a VOID_AUTHORED block at cells (8..11, 8..11) -> impassable.
 *
 * Dual (a modelled underside at -20 m), so the §3.4 bottom clamp is live and
 * this is not the degenerate legacy page.
 */
zref::render::TerrainPatch make_patch() {
  zref::render::TerrainPatch p;
  p.width = kLat;
  p.height = kLat;
  p.env_x0 = 0;
  p.env_z0 = 0;
  p.env_x1 = kSpan;
  p.env_z1 = kSpan;
  p.heights.assign(static_cast<size_t>(kLat) * kLat, 0);
  p.bottom.assign(static_cast<size_t>(kLat) * kLat, static_cast<int16_t>(-20 * 256));
  p.cell_state.assign(static_cast<size_t>(kCells) * kCells, zref::terrain::kSolid);

  // the ridge: heights rise 6 m per lattice step across columns 24..26
  for (int j = 0; j < kLat; ++j) {
    for (int i = 24; i < kLat; ++i) {
      const int steps = (i - 23) > 3 ? 3 : (i - 23);
      p.heights[static_cast<size_t>(j) * kLat + i] = static_cast<int16_t>(steps * 6 * 256);
    }
  }
  // the void block
  for (int cj = 8; cj <= 11; ++cj)
    for (int ci = 8; ci <= 11; ++ci)
      p.cell_state[static_cast<size_t>(cj) * kCells + ci] = zref::terrain::kVoidAuthored;
  return p;
}

ZhTransform2fx identity_xform() {
  ZhTransform2fx x{};
  x.tx = 0;
  x.ty = 0;
  x.r00 = kOne;
  x.r01 = 0;
  x.r10 = 0;
  x.r11 = kOne;
  return x;
}

// ---------------------------------------------------------------- programs --

/**
 * An earth program whose four out-lanes are constants. STIMULUS: it lets a
 * chosen delta reach `compose_nav` so the reduction can be checked at a value
 * a human can verify, which a shaped spell cannot do.
 */
zfield::Decoded const_prog(int32_t height_fx, int32_t nav_fx, int out_lane_count = 4) {
  zfield::Decoded p;
  p.profile = zfield::EARTH;
  const int32_t vals[4] = {height_fx, 0, 0, nav_fx};
  for (int k = 0; k < 4; ++k) {
    zfield::Instr ins{};
    ins.op = zfield::OP_LDC;
    ins.dst = static_cast<uint8_t>(16 + k);
    ins.imm = static_cast<uint32_t>(vals[k]);
    p.instrs.push_back(ins);
  }
  zfield::Instr end{};
  end.op = zfield::OP_END;
  p.instrs.push_back(end);
  // earth in-lanes: x, z, age, phase, p0..p7 -> regs 0..11 in order
  static const char* kIn[12] = {"x", "z", "age", "phase", "p0", "p1",
                                "p2", "p3", "p4", "p5", "p6", "p7"};
  for (int i = 0; i < 12; ++i) {
    zfield::IoLane l{};
    l.name = kIn[i];
    l.type = (i == 2) ? 3 : 0;
    l.reg = static_cast<uint8_t>(i);
    p.in_lanes.push_back(l);
  }
  static const char* kOut[4] = {"height", "velocity", "material", "nav_cost"};
  for (int k = 0; k < out_lane_count; ++k) {
    zfield::IoLane l{};
    l.name = kOut[k];
    l.type = (k == 2) ? 3 : 0;
    l.reg = static_cast<uint8_t>(16 + k);
    p.out_lanes.push_back(l);
  }
  return p;
}

/** nav_cost = age (in-lane 2). The ANIMATED case: its answer must move per tick. */
zfield::Decoded age_nav_prog() {
  zfield::Decoded p = const_prog(0, 0);
  p.instrs.clear();
  for (int k = 0; k < 3; ++k) {
    zfield::Instr ins{};
    ins.op = zfield::OP_LDC;
    ins.dst = static_cast<uint8_t>(16 + k);
    ins.imm = 0;
    p.instrs.push_back(ins);
  }
  zfield::Instr mov{};
  mov.op = zfield::OP_MOV;
  mov.dst = 19;
  mov.a = 2;  // age
  p.instrs.push_back(mov);
  zfield::Instr end{};
  end.op = zfield::OP_END;
  p.instrs.push_back(end);
  return p;
}

ZhCmdTerrainField cmd_over(int32_t x0, int32_t z0, int32_t x1, int32_t z1, uint32_t start,
                           uint32_t dur) {
  ZhCmdTerrainField c{};
  c.program = 1;
  c.footprint.x0 = x0;
  c.footprint.y0 = z0;
  c.footprint.x1 = x1;
  c.footprint.y1 = z1;
  c.start_tick = start;
  c.duration_ticks = dur;
  return c;
}

/** The whole patch, as a footprint. */
ZhCmdTerrainField cmd_all(uint32_t start = 0, uint32_t dur = 1000000) {
  return cmd_over(0, 0, kSpan, kSpan, start, dur);
}

// ------------------------------------------------------------------ router --

/**
 * THE ROUTE-SELECTION HARNESS. A 4-neighbour Dijkstra over cells whose ONLY
 * source of truth is `Service::query_cell` -- passability and cost both. It is
 * a HARNESS and is deliberately not production: the owner's fence says this
 * packet is not a pathfinding framework. It exists to show that a consumer
 * shaped like the real ones answers differently when the costs move.
 */
struct Route {
  bool found = false;
  int64_t cost = 0;
  int steps = 0;
  std::vector<int> cells;  // cj*kCells+ci, start..goal
};

Route route(const zref::nav::Service& nav, int sci, int scj, int gci, int gcj) {
  const int n = kCells * kCells;
  std::vector<int64_t> dist(static_cast<size_t>(n), INT64_MAX);
  std::vector<int> prev(static_cast<size_t>(n), -1);
  using Node = std::pair<int64_t, int>;
  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;
  const int s = scj * kCells + sci, g = gcj * kCells + gci;
  dist[static_cast<size_t>(s)] = 0;
  pq.push({0, s});
  while (!pq.empty()) {
    const Node top = pq.top();
    pq.pop();
    if (top.first != dist[static_cast<size_t>(top.second)]) continue;
    if (top.second == g) break;
    const int ci = top.second % kCells, cj = top.second / kCells;
    static const int dx[4] = {1, -1, 0, 0};
    static const int dz[4] = {0, 0, 1, -1};
    for (int d = 0; d < 4; ++d) {
      const int ni = ci + dx[d], nj = cj + dz[d];
      if (ni < 0 || nj < 0 || ni >= kCells || nj >= kCells) continue;
      const zref::nav::Result r = nav.query_cell(ni, nj);
      if (!r.passable) continue;  // hard passability: never routed through
      const int nid = nj * kCells + ni;
      const int64_t nd = top.first + r.cost;
      if (nd >= dist[static_cast<size_t>(nid)]) continue;
      dist[static_cast<size_t>(nid)] = nd;
      prev[static_cast<size_t>(nid)] = top.second;
      pq.push({nd, nid});
    }
  }
  Route out;
  if (dist[static_cast<size_t>(g)] == INT64_MAX) return out;
  out.found = true;
  out.cost = dist[static_cast<size_t>(g)];
  for (int c = g; c != -1; c = prev[static_cast<size_t>(c)]) out.cells.push_back(c);
  out.steps = static_cast<int>(out.cells.size()) - 1;
  return out;
}

bool route_touches_row(const Route& r, int cj) {
  for (int c : r.cells)
    if (c / kCells == cj) return true;
  return false;
}

// =========================================================== acceptance 1 ===
// "No-field results match the authored baseline."

void t1_no_field_baseline() {
  std::printf("1. no-field results match the authored baseline\n");
  const zref::render::TerrainPatch patch = make_patch();
  zref::nav::Service nav;
  nav.set_terrain(&patch, identity_xform());
  nav.begin_tick(0);

  // level ground, far from the ridge: level means zero slope surcharge, so the
  // answer must be flat_cost EXACTLY -- an identity, not a tolerance.
  const zref::nav::Result r = nav.query_cell(4, 4);
  check(r.passable, "level ground is passable");
  check_eq(r.cost, nav.policy().flat_cost, "cost == flat_cost on level ground with no field");
  check_eq(r.authored_cost, r.cost, "cost == authored_cost with no field");
  check_eq(r.fields_covering, 0, "no field covers anything");
  check_eq(r.fields_applied, 0, "no field contributes a delta");

  // every passable cell agrees, everywhere on the patch
  int passable = 0, mismatched = 0;
  for (int cj = 0; cj < kCells; ++cj)
    for (int ci = 0; ci < kCells; ++ci) {
      const zref::nav::Result c = nav.query_cell(ci, cj);
      if (!c.passable) continue;
      ++passable;
      if (c.cost != c.authored_cost) ++mismatched;
    }
  check(passable > 700, "most of the fixture is walkable");
  check_eq(mismatched, 0, "cost == authored_cost on EVERY passable cell with no field");

  // [control] the same sweep with a field live must NOT be all-equal, or the
  // assertion above is an equality that could never fail.
  const zfield::Decoded prog = const_prog(0, kOne / 2);
  zref::nav::Service nav2;
  nav2.set_terrain(&patch, identity_xform());
  nav2.begin_tick(0);
  nav2.add_field(&prog, cmd_all());
  int moved = 0;
  for (int cj = 0; cj < kCells; ++cj)
    for (int ci = 0; ci < kCells; ++ci) {
      const zref::nav::Result c = nav2.query_cell(ci, cj);
      if (c.passable && c.cost != c.authored_cost) ++moved;
    }
  check(moved > 700, "[control] with a field live the same sweep DOES move");

  // the policy is a knob and it moves the baseline
  zref::nav::Policy p = nav.policy();
  p.flat_cost = 3 * kOne;
  nav.set_policy(p);
  check_eq(nav.query_cell(4, 4).cost, 3 * kOne, "flat_cost is a live knob");
}

// =========================================================== acceptance 2 ===
// "A nonzero field changes the queried cost in its covered region."

void t2_field_changes_cost() {
  std::printf("2. a nonzero field changes the queried cost in its covered region\n");
  const zref::render::TerrainPatch patch = make_patch();

  // -- 2a: a REAL SHIPPED SPELL --------------------------------------------
  const zfield::DecodeResult dr = zfield::decode(zfield_gen::crater_ring::kProgramBytes.data(),
                                                 zfield_gen::crater_ring::kProgramBytesLen);
  check(dr.error == zfield::DecodeError::kOk, "crater_ring decodes");
  check_eq(static_cast<int64_t>(dr.prog.out_lanes.size()), 4,
           "crater_ring declares four out-lanes (nav_cost is lane 3)");

  zref::nav::Service nav;
  nav.set_terrain(&patch, identity_xform());
  nav.begin_tick(0);
  // a crater over the middle third, with real parameters: radius and amplitude
  ZhCmdTerrainField c = cmd_over(16 * kOne, 16 * kOne, 48 * kOne, 48 * kOne, 0, 120);
  const int32_t params[8] = {24 * kOne, 24 * kOne, 8 * kOne, 3 * kOne, kOne, kOne, 0, 0};
  std::memcpy(c.parameters, params, sizeof(params));
  const uint32_t id = nav.add_field(&dr.prog, c);
  check(id != 0, "the crater is accepted by the §9.1 intake");
  nav.begin_tick(40);

  // The footprint is [16 m, 48 m] on both axes and the pitch is 2 m, so the
  // COVERED VERTICES are lattice indices 8..24 (the §9.1 test is CLOSED, so
  // both ends are inside). A cell owns corners {ci, ci+1}, therefore:
  //   fully inside  : ci in [8, 23]
  //   fully outside : ci <= 6 or ci >= 25  (neither corner is covered)
  //   the two rings between them are BOUNDARY cells and are classified as
  //   neither -- getting this wrong is what made the first version of this
  //   assertion fail, and the service was right both times.
  int inside_moved = 0, inside_covered = 0, outside_moved = 0, outside_cells = 0;
  auto band = [](int c) { return c >= 8 && c <= 23 ? 1 : (c <= 6 || c >= 25 ? -1 : 0); };
  for (int cj = 0; cj < kCells; ++cj)
    for (int ci = 0; ci < kCells; ++ci) {
      const zref::nav::Result r = nav.query_cell(ci, cj);
      if (!r.passable) continue;
      const int bi = band(ci), bj = band(cj);
      if (bi == 1 && bj == 1) {
        if (r.fields_covering > 0) ++inside_covered;
        if (r.cost != r.authored_cost) ++inside_moved;
      } else if (bi == -1 || bj == -1) {
        ++outside_cells;
        if (r.cost != r.authored_cost) ++outside_moved;
      }
    }
  check(inside_covered > 100, "the crater's footprint covers its region");
  check(outside_cells > 200, "there are plenty of cells outside the footprint to check");
  check_eq(outside_moved, 0, "and NOTHING outside the footprint moved");
  check(inside_moved > 0, "a REAL shipped spell moves the queried cost inside its footprint");
  std::printf("     crater_ring: %d cells covered, %d cells whose cost moved\n", inside_covered,
              inside_moved);

  // -- 2b: a controlled delta, checked against compose_nav by hand ----------
  const zfield::Decoded prog = const_prog(0, kOne);  // +1.0 everywhere it covers
  zref::nav::Service n2;
  n2.set_terrain(&patch, identity_xform());
  n2.begin_tick(0);
  n2.add_field(&prog, cmd_over(0, 0, 20 * kOne, 20 * kOne, 0, 1000));
  const zref::nav::Result in = n2.query_cell(2, 2);
  const zref::nav::Result out = n2.query_cell(28, 2);
  check_eq(in.cost, zref::fieldir::compose_nav(n2.policy().flat_cost, nullptr, 0) + kOne,
           "covered cell: authored + delta, by compose_nav");
  check_eq(in.fields_applied, 1, "one lane contributed");
  check_eq(out.fields_covering, 0, "a cell outside the footprint is not covered");
  check_eq(out.cost, out.authored_cost, "a cell outside the footprint is untouched");
}

// =========================================================== acceptance 3 ===
// "A small route-selection test using that same API responds to the changed
//  costs, then responds correctly when the field expires or is removed."

void t3_route_selection() {
  std::printf("3. route selection responds to cost, to expiry and to removal\n");
  const zref::render::TerrainPatch patch = make_patch();
  zref::nav::Service nav;
  nav.set_terrain(&patch, identity_xform());
  nav.begin_tick(0);

  // start and goal on row 16, both clear of the ridge (columns 24+) and of the
  // void block (cells 8..11); the direct path is along row 16.
  const int sci = 1, scj = 16, gci = 22, gcj = 16;
  const Route base = route(nav, sci, scj, gci, gcj);
  check(base.found, "a route exists with no field");
  check(route_touches_row(base, 16), "the cheap route runs straight along row 16");
  const int64_t base_cost = base.cost;

  // a barrier field over rows 15..17: expensive, not blocking.
  const zfield::Decoded prog = const_prog(0, 4 * kOne);
  const int32_t z0 = 15 * kPitchM * kOne, z1 = 18 * kPitchM * kOne;
  const uint32_t start = 10, dur = 30;
  zref::nav::Service& m = nav;
  m.begin_tick(start);
  const uint32_t id = m.add_field(&prog, cmd_over(0, z0, kSpan, z1, start, dur));
  check(id != 0, "the barrier is accepted");

  const Route detour = route(m, sci, scj, gci, gcj);
  check(detour.found, "a route still exists across an expensive band");
  check(detour.steps > base.steps, "the router now takes a LONGER path in steps");
  check(detour.cost > base_cost, "and it costs more than the free-field route did");
  check(!route_touches_row(detour, 16) || detour.steps > base.steps,
        "the router leaves the expensive band wherever it can");
  std::printf("     base: %d steps cost %lld | detour: %d steps cost %lld\n", base.steps,
              static_cast<long long>(base_cost), detour.steps,
              static_cast<long long>(detour.cost));

  // EXPIRY: past start_tick + duration the instance retires and the answer
  // returns to the authored baseline -- and the route with it.
  m.begin_tick(start + dur + 1);
  check_eq(m.active_fields(), 0, "the instance retired at start_tick + duration");
  const Route after_expiry = route(m, sci, scj, gci, gcj);
  check(after_expiry.found, "a route exists after expiry");
  check_eq(after_expiry.cost, base_cost, "expiry restores the baseline route cost exactly");
  check_eq(after_expiry.steps, base.steps, "expiry restores the baseline route length");

  // REMOVAL: the same, by explicit dispel rather than by the clock.
  zref::nav::Service r2;
  r2.set_terrain(&patch, identity_xform());
  r2.begin_tick(0);
  const uint32_t rid = r2.add_field(&prog, cmd_over(0, z0, kSpan, z1, 0, 1000000));
  const Route blocked2 = route(r2, sci, scj, gci, gcj);
  check(blocked2.cost > base_cost, "the same barrier raises the route cost again");
  check(r2.remove_field(rid), "remove_field finds the instance");
  check_eq(r2.active_fields(), 0, "and drops it");
  const Route after_remove = route(r2, sci, scj, gci, gcj);
  check_eq(after_remove.cost, base_cost, "removal restores the baseline route cost exactly");

  // [control] removing a field that is not there must not silently succeed.
  check(!r2.remove_field(rid), "[control] removing a retired id returns false");
}

// =========================================================== acceptance 4 ===
// "Hard-blocked terrain remains blocked, including with negative cost deltas."

void t4_blocked_stays_blocked() {
  std::printf("4. hard-blocked terrain stays blocked, negative deltas included\n");
  const zref::render::TerrainPatch patch = make_patch();
  zref::nav::Service nav;
  nav.set_terrain(&patch, identity_xform());
  nav.begin_tick(0);

  const zref::nav::Result v = nav.query_cell(9, 9);
  check(!v.passable, "a VOID_AUTHORED cell is impassable");
  check(v.block == zref::nav::Block::kVoid, "and it says WHY: kVoid");
  check_eq(v.cost, zref::nav::kBlockedCost, "a blocked cell reports the most expensive cost");

  const zref::nav::Result s = nav.query_cell(24, 4);
  check(!s.passable, "the ridge is too steep to walk");
  check(s.block == zref::nav::Block::kSlope, "and it says WHY: kSlope");

  const zref::nav::Result o = nav.query(zref::fx16{-kOne}, zref::fx16{4 * kOne});
  check(!o.passable, "outside the envelope is impassable");
  check(o.block == zref::nav::Block::kOut, "and it says WHY: kOut");

  // the attack: a field whose nav delta is hugely NEGATIVE over the whole patch
  const zfield::Decoded cheap = const_prog(0, -1000 * kOne);
  nav.add_field(&cheap, cmd_all());
  nav.begin_tick(1);
  check(nav.active_fields() == 1, "the cheapening field is live");

  const zref::nav::Result v2 = nav.query_cell(9, 9);
  const zref::nav::Result s2 = nav.query_cell(24, 4);
  check(!v2.passable, "a VOID cell is STILL impassable under a -1000 delta");
  check(!s2.passable, "the ridge is STILL impassable under a -1000 delta");
  check(v2.block == zref::nav::Block::kVoid, "the void's reason is unchanged");
  check(s2.block == zref::nav::Block::kSlope, "the ridge's reason is unchanged");

  // [control] the delta DID reach lawful ground -- otherwise the two
  // assertions above hold because nothing happened at all.
  const zref::nav::Result flat = nav.query_cell(4, 4);
  check(flat.passable, "[control] lawful ground is still passable");
  check_eq(flat.cost, 0, "[control] and the -1000 delta floored its cost at zero");
  check_eq(flat.fields_applied, 1, "[control] the delta really was applied there");

  // and no route may pass through the void or the ridge even when they are free
  const Route r = route(nav, 4, 9, 20, 9);
  check(r.found, "a route exists across the patch");
  for (int c : r.cells) {
    const int ci = c % kCells, cj = c / kCells;
    check(!(ci >= 8 && ci <= 11 && cj >= 8 && cj <= 11), "no route cell is inside the void block");
  }
  const Route across = route(nav, 4, 4, 30, 4);
  check(!across.found, "no route crosses the impassable ridge, at ANY cost");
}

// =========================================================== acceptance 5 ===
// "Overlapping fields, saturation, command order and generation changes follow
//  the existing rules."

void t5_overlap_saturation_order_generation() {
  std::printf("5. overlap, saturation, command order and generation\n");
  const zref::render::TerrainPatch patch = make_patch();
  const int32_t base_flat = kOne;

  // -- overlap: two fields over one cell ADD, they do not replace ------------
  {
    const zfield::Decoded a = const_prog(0, kOne);
    const zfield::Decoded b = const_prog(0, 2 * kOne);
    zref::nav::Service nav;
    nav.set_terrain(&patch, identity_xform());
    nav.begin_tick(0);
    nav.add_field(&a, cmd_over(0, 0, 20 * kOne, 20 * kOne, 0, 1000));
    nav.add_field(&b, cmd_over(0, 0, 10 * kOne, 10 * kOne, 0, 1000));
    const zref::nav::Result both = nav.query_cell(2, 2);   // inside both
    const zref::nav::Result one = nav.query_cell(7, 2);    // inside a only
    check_eq(both.cost, base_flat + 3 * kOne, "overlapping deltas ADD (1.0 + 2.0)");
    check_eq(both.fields_applied, 2, "two lanes contributed");
    check_eq(one.cost, base_flat + kOne, "the wider field alone contributes 1.0");
    check_eq(one.fields_applied, 1, "one lane contributed");
  }

  // -- saturation: the chain clamps at INT32_MAX and never wraps -------------
  {
    const zfield::Decoded big = const_prog(0, INT32_MAX);
    zref::nav::Service nav;
    nav.set_terrain(&patch, identity_xform());
    nav.begin_tick(0);
    nav.add_field(&big, cmd_all());
    nav.add_field(&big, cmd_all());
    nav.add_field(&big, cmd_all());
    const zref::nav::Result r = nav.query_cell(4, 4);
    check_eq(r.fields_applied, 3, "three saturating lanes contributed");
    check_eq(r.cost, INT32_MAX, "three INT32_MAX deltas SATURATE, they do not wrap");
    check(r.cost > 0, "and the result never goes negative through a wrap");
  }

  // -- command order: it is observable, at saturation ------------------------
  // compose_nav clamps to the int32 range after EVERY delta and floors at zero
  // ONCE, on the way out. So {+MAX, -MAX/2} lands at MAX/2 + 1 while
  // {-MAX/2, +MAX} lands at MAX. Same two numbers, different order, different
  // answer -- which is exactly why the accepted list must stay in command
  // order and why erase() rather than swap-and-pop is used to remove from it.
  {
    const zfield::Decoded pos = const_prog(0, INT32_MAX);
    const zfield::Decoded neg = const_prog(0, -(INT32_MAX / 2));
    zref::nav::Service ab, ba;
    ab.set_terrain(&patch, identity_xform());
    ba.set_terrain(&patch, identity_xform());
    ab.begin_tick(0);
    ba.begin_tick(0);
    ab.add_field(&pos, cmd_all());
    ab.add_field(&neg, cmd_all());
    ba.add_field(&neg, cmd_all());
    ba.add_field(&pos, cmd_all());
    const int32_t cab = ab.query_cell(4, 4).cost;
    const int32_t cba = ba.query_cell(4, 4).cost;
    check(cab != cba, "command ORDER is observable in the composed cost");
    // and both agree with the ratified function given the same ordered deltas
    const int32_t d_ab[2] = {INT32_MAX, -(INT32_MAX / 2)};
    const int32_t d_ba[2] = {-(INT32_MAX / 2), INT32_MAX};
    check_eq(cab, zref::fieldir::compose_nav(base_flat, d_ab, 2), "order A,B matches compose_nav");
    check_eq(cba, zref::fieldir::compose_nav(base_flat, d_ba, 2), "order B,A matches compose_nav");
  }

  // -- presence: an ABSENT out-lane is not a write of zero -------------------
  {
    const zfield::Decoded three = const_prog(0, kOne, /*out_lane_count=*/3);
    zref::nav::Service nav;
    nav.set_terrain(&patch, identity_xform());
    nav.begin_tick(0);
    nav.add_field(&three, cmd_all());
    const zref::nav::Result r = nav.query_cell(4, 4);
    check(r.fields_covering == 1, "a three-lane program still COVERS the cell");
    check_eq(r.fields_applied, 0, "but it contributes NO delta: an absent lane is not a zero");
    check_eq(r.cost, r.authored_cost, "so the cost is the authored baseline");
    // [control] the same program with the fourth lane declared DOES contribute
    const zfield::Decoded four = const_prog(0, kOne, 4);
    zref::nav::Service n2;
    n2.set_terrain(&patch, identity_xform());
    n2.begin_tick(0);
    n2.add_field(&four, cmd_all());
    check_eq(n2.query_cell(4, 4).fields_applied, 1, "[control] four out-lanes DO contribute");
  }

  // -- generation: the tick is part of it while a field is live -------------
  {
    const zfield::Decoded ageing = age_nav_prog();
    zref::nav::Service nav;
    nav.set_terrain(&patch, identity_xform());
    nav.begin_tick(0);
    nav.add_field(&ageing, cmd_all(0, 1000));
    const uint64_t g10 = (nav.begin_tick(10), nav.generation());
    const int32_t c10 = nav.query_cell(4, 4).cost;
    const uint64_t g40 = (nav.begin_tick(40), nav.generation());
    const int32_t c40 = nav.query_cell(4, 4).cost;
    check(g10 != g40, "the generation moves with the tick while a field is listed");
    check(c40 > c10, "an ANIMATED field's cost evolves per tick (not frozen by a stamp rule)");
    check_eq(c10 - base_flat, 10, "nav_cost == age at tick 10");
    check_eq(c40 - base_flat, 40, "nav_cost == age at tick 40");

    // and with NO field listed the tick does NOT churn the cache
    nav.clear_fields();
    nav.begin_tick(100);
    const uint64_t rb = nav.rebuilds();
    (void)nav.query_cell(4, 4);
    nav.begin_tick(101);
    (void)nav.query_cell(4, 4);
    nav.begin_tick(102);
    (void)nav.query_cell(4, 4);
    check_eq(static_cast<int64_t>(nav.rebuilds() - rb), 1,
             "with no field listed the tick alone does not rebuild");

    // terrain_changed DOES invalidate, and that is the other half of coherence
    const uint64_t rb2 = nav.rebuilds();
    nav.terrain_changed();
    (void)nav.query_cell(4, 4);
    check_eq(static_cast<int64_t>(nav.rebuilds() - rb2), 1, "terrain_changed invalidates");
  }

  // -- the §9.1 intake bound is enforced, counted, and never silent ---------
  {
    const zfield::Decoded p = const_prog(0, kOne);
    zref::nav::Service nav;
    nav.set_terrain(&patch, identity_xform());
    nav.begin_tick(0);
    for (int i = 0; i < 20; ++i) nav.add_field(&p, cmd_all());
    check_eq(nav.active_fields(), zref::terrain::kMaxPatchFields, "intake accepts exactly 16");
    check_eq(nav.fields_rejected(), 4, "and REJECTS the other four, counted not dropped");
    check_eq(nav.query_cell(4, 4).cost, kOne + 16 * kOne, "16 lanes, and only 16, compose");
  }
}

// ============================================================ measurement ===

void t6_measure() {
  std::printf("6. CPU work and memory, on the measuring machine\n");
  const zref::render::TerrainPatch patch = make_patch();
  const zfield::DecodeResult dr = zfield::decode(zfield_gen::crater_ring::kProgramBytes.data(),
                                                 zfield_gen::crater_ring::kProgramBytesLen);
  zref::nav::Service nav;
  nav.set_terrain(&patch, identity_xform());

  ZhCmdTerrainField c = cmd_over(16 * kOne, 16 * kOne, 48 * kOne, 48 * kOne, 0, 100000);
  const int32_t params[8] = {24 * kOne, 24 * kOne, 8 * kOne, 3 * kOne, kOne, kOne, 0, 0};
  std::memcpy(c.parameters, params, sizeof(params));
  for (int i = 0; i < 4; ++i) nav.add_field(&dr.prog, c);  // 4 live spells on one patch

  using clk = std::chrono::steady_clock;

  // (a) a whole-tick rebuild: one compose_lattice over 1,089 vertices x 4 fields
  const int kTicks = 200;
  const auto t0 = clk::now();
  for (int t = 0; t < kTicks; ++t) {
    nav.begin_tick(static_cast<uint32_t>(t));
    (void)nav.query_cell(4, 4);  // the first query of the tick pays the rebuild
  }
  const auto t1 = clk::now();
  const double per_tick_us =
      std::chrono::duration<double, std::micro>(t1 - t0).count() / kTicks;

  // (b) steady-state queries at a settled generation
  const int kQ = 200000;
  const auto t2 = clk::now();
  int64_t sink = 0;
  for (int q = 0; q < kQ; ++q) {
    const zref::nav::Result r = nav.query_cell(q % kCells, (q / kCells) % kCells);
    sink += r.cost;
  }
  const auto t3 = clk::now();
  const double per_query_ns = std::chrono::duration<double, std::nano>(t3 - t2).count() / kQ;

  std::printf("     rebuild + first query : %8.1f us / tick  (4 live fields, 33x33 lattice)\n",
              per_tick_us);
  std::printf("     steady-state query    : %8.1f ns / query (cache warm)\n", per_query_ns);
  std::printf("     cache                 : %8zu bytes for one patch\n", nav.cache_bytes());
  std::printf("     rebuilds / queries    : %llu / %llu\n",
              static_cast<unsigned long long>(nav.rebuilds()),
              static_cast<unsigned long long>(nav.queries()));
  std::printf("     TARGET: x86-64 DESKTOP (this build host). ARM/HPS performance is\n");
  std::printf("             UNVERIFIED and must not be inferred from these numbers.\n");
  check(sink != 0 || sink == 0, "measurement ran");
  // EXACTLY one rebuild per tick and NONE in the 200,000-query loop. This is
  // the counter that proves the steady-state number above is a cached query and
  // not a hidden re-evaluation -- "an apparently cheap API" is exactly what the
  // owner told this packet not to hide a full-world walk behind.
  check_eq(static_cast<int64_t>(nav.rebuilds()), kTicks,
           "exactly one rebuild per tick, and none in the steady-state loop");
}

}  // namespace

int main() {
  std::printf("nav_service_directed: SW.CPUCOLL navigation query, production API\n");
  t1_no_field_baseline();
  t2_field_changes_cost();
  t3_route_selection();
  t4_blocked_stays_blocked();
  t5_overlap_saturation_order_generation();
  t6_measure();
  std::printf("nav_service_directed: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
