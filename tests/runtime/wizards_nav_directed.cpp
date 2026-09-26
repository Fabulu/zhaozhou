// wizards_nav_directed.cpp — the WIRED navigation path: `zgame::Wizards`, the
// console's only `zcon::GameTruth`, consuming `zref::nav::Service` through the
// runtime boundary, driven by a real `zcon::Session`.
//
// WHY THIS FILE EXISTS SEPARATELY FROM tests/nav/nav_service_directed.cpp.
// That one proves the SERVICE against the owner's five acceptance items. This
// one proves the INTEGRATION, which is a different claim and the one the owner
// made the harder bar:
//
//   > Expose the service through the runtime interface that Form simulation and
//   > game AI can actually call. A new reference-only helper, debug counter, or
//   > testbench-only read is not completion.
//
// A service with a perfect test suite and no caller is exactly the shape the
// same decision struck on the FPGA side -- `nav_cost_o` was computed correctly
// for its whole life and nothing read it. So the assertions here are about a
// WIZARD: where one can walk, how fast, and whether a recording of it replays.
//
// AND THE HOOK IS PROVEN LOAD-BEARING. `zcon::TickObserver` was added for this
// packet; a hook nothing needs is a hook that cannot fail, so section 5 replays
// the same recording WITHOUT the observer and requires a DIVERGENCE. That is
// the positive control for the mechanism, kept separate from the correctness
// assertion beside it.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "zcon/zcon.hpp"
#include "zgame/wizards.hpp"

#include "zfield/zfield.hpp"
#include "zref/zref_nav.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_terrain.hpp"

namespace {

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

constexpr int kLat = 33;
constexpr int kCells = kLat - 1;
constexpr int32_t kOne = 1 << 16;
constexpr int32_t kHalfFx = 16 * kOne;  // the wizard field is 32 m, centred

/**
 * The same world shape the desktop host builds: 33x33 over 32 m at 1 m pitch,
 * one lattice cell per game grid cell, with a VOID CHASM at cells x in [10,11].
 * Written here rather than shared with the host because it is a FIXTURE -- the
 * law it exercises lives in the service, not in the shape of this terrain.
 */
struct World : public zcon::TickObserver {
  zref::render::TerrainPatch patch;
  zref::nav::Service service;
  zfield::Decoded mire;
  uint32_t mire_id = 0;
  void before_tick(uint32_t tick) override { service.begin_tick(tick); }
};

zfield::Decoded const_nav_prog(int32_t nav_fx) {
  zfield::Decoded p;
  p.profile = zfield::EARTH;
  const int32_t vals[4] = {0, 0, 0, nav_fx};
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
  for (int k = 0; k < 4; ++k) {
    zfield::IoLane l{};
    l.name = kOut[k];
    l.type = (k == 2) ? 3 : 0;
    l.reg = static_cast<uint8_t>(16 + k);
    p.out_lanes.push_back(l);
  }
  return p;
}

void build(World* w, bool with_mire, bool with_chasm = true) {
  zref::render::TerrainPatch& p = w->patch;
  p.width = kLat;
  p.height = kLat;
  p.env_x0 = -kHalfFx;
  p.env_z0 = -kHalfFx;
  p.env_x1 = kHalfFx;
  p.env_z1 = kHalfFx;
  p.heights.assign(static_cast<size_t>(kLat) * kLat, 0);
  p.bottom.assign(static_cast<size_t>(kLat) * kLat, static_cast<int16_t>(-20 * 256));
  p.cell_state.assign(static_cast<size_t>(kCells) * kCells, zref::terrain::kSolid);
  if (with_chasm)
    for (int cj = 0; cj < kCells; ++cj)
      for (int ci = 10; ci <= 11; ++ci)
        p.cell_state[static_cast<size_t>(cj) * kCells + ci] = zref::terrain::kVoidAuthored;

  zhao_abi::ZhTransform2fx xf{};
  xf.r00 = kOne;
  xf.r11 = kOne;
  w->service.set_terrain(&p, xf);
  w->service.begin_tick(0);

  if (!with_mire) return;
  w->mire = const_nav_prog(3 * kOne);
  zhao_abi::ZhCmdTerrainField cmd{};
  cmd.program = 1;
  cmd.footprint.x0 = -kHalfFx;
  cmd.footprint.y0 = -kHalfFx;
  cmd.footprint.x1 = kHalfFx;
  cmd.footprint.y1 = kHalfFx;
  cmd.start_tick = 0;
  cmd.duration_ticks = 40;  // it EXPIRES, and the test walks past that tick
  w->mire_id = w->service.add_field(&w->mire, cmd);
}

/** A scripted backend: both pads push a constant direction. */
class ScriptBackend : public zcon::Backend {
 public:
  ScriptBackend(int8_t lx, int8_t ly) : lx_(lx), ly_(ly) {}
  zcon::Handle publish(zcon::ResourceKind, const uint8_t*, std::size_t) override { return {}; }
  zcon::InputSnapshot poll(uint32_t tick) override {
    zcon::InputSnapshot in{};
    in.tick = tick;
    for (int p = 0; p < zcon::InputSnapshot::kPads; ++p) {
      in.pad[p].stick_lx = lx_;
      in.pad[p].stick_ly = ly_;
    }
    return in;
  }
  void submit(const std::vector<uint8_t>&) override {}
  const char* name() const override { return "script"; }

 private:
  int8_t lx_, ly_;
};

// The wizard's game position of the chasm: cells 10..11 of a 32-cell grid, one
// game metre each, so game x in [10, 12) metres -> [10*kOne, 12*kOne).
constexpr int32_t kChasmLo = 10 * zgame::kOne;
constexpr int32_t kChasmHi = 12 * zgame::kOne;

// ============================================================================

void t1_unnavigated_walks_into_the_hole() {
  std::printf("1. [control] with NO service attached the wizard walks into the chasm\n");
  zgame::Wizards truth;
  truth.reset(1);
  ScriptBackend be(100, 0);  // push +x, hard
  zcon::Session s(&truth, &be);
  s.start(1);
  for (int t = 0; t < 60; ++t) s.tick();
  const int32_t x = truth.wizard(0).x;
  check(x > kChasmHi, "[control] the un-navigated wizard crossed the chasm entirely");
  check_eq(truth.nav_refusals(), 0, "[control] and refused nothing, because nothing was asked");
  std::printf("     un-navigated p0 ended at game x = %d (chasm is %d..%d)\n", x, kChasmLo,
              kChasmHi);
}

void t2_navigated_stops_at_the_edge() {
  std::printf("2. with the service attached the wizard is REFUSED at the chasm\n");
  World w;
  build(&w, /*with_mire=*/false);
  zgame::Wizards truth;
  truth.reset(1);
  truth.set_nav(&w.service, 16 * zgame::kOne);
  ScriptBackend be(100, 0);
  zcon::Session s(&truth, &be);
  s.set_tick_observer(&w);
  s.start(1);
  int32_t worst = 0;
  for (int t = 0; t < 60; ++t) {
    s.tick();
    const int32_t x = truth.wizard(0).x;
    if (x > worst) worst = x;
    // the hard assertion: at no tick may the wizard stand on impassable ground
    const zref::nav::Result r =
        w.service.query(truth.world_x(truth.wizard(0).x), truth.world_z(truth.wizard(0).y));
    check(r.passable, "the wizard never stands on an impassable cell");
  }
  check(worst < kChasmLo + zgame::kOne, "the wizard stopped at the near lip of the chasm");
  check(truth.nav_refusals() > 0, "and navigation REFUSED steps -- the counter moved");
  std::printf("     navigated p0 stopped at game x = %d, %u refusals\n", worst,
              truth.nav_refusals());
}

void t3_cost_slows_and_expiry_restores() {
  std::printf("3. composed cost SLOWS the walk, and expiry restores the speed\n");
  // OPEN GROUND, no chasm, and a GENTLE stick. Both matter, and the first
  // version of this test got it wrong in an instructive way: with a wall ahead,
  // the SLOWER wizard ended up FURTHER because its finer steps fitted closer to
  // the lip before one was refused. "Slower" and "covers less ground" are only
  // the same statement where nothing is in the way, so the speed measurement
  // gets clear ground and the refusal measurement (section 2) gets the chasm.
  constexpr int8_t kStick = 10;          // dx = 40 per tick at cost 1.0
  constexpr int kWindow = 20;

  World clean;
  build(&clean, /*with_mire=*/false, /*with_chasm=*/false);
  zgame::Wizards a;
  a.set_nav(&clean.service, 16 * zgame::kOne);
  ScriptBackend be(kStick, 0);
  zcon::Session sa(&a, &be);
  sa.set_tick_observer(&clean);
  sa.start(1);
  const int32_t start_x = a.wizard(0).x;
  for (int t = 0; t < kWindow; ++t) sa.tick();
  const int32_t clean_dist = a.wizard(0).x - start_x;

  World mired;
  build(&mired, /*with_mire=*/true, /*with_chasm=*/false);
  zgame::Wizards b;
  b.set_nav(&mired.service, 16 * zgame::kOne);
  zcon::Session sb(&b, &be);
  sb.set_tick_observer(&mired);
  sb.start(1);
  for (int t = 0; t < kWindow; ++t) sb.tick();
  const int32_t mired_dist = b.wizard(0).x - start_x;

  check(clean_dist > 0, "the clean wizard moved");
  check(mired_dist > 0, "the mired wizard moved too -- a mire is not a wall");
  check(mired_dist < clean_dist, "but LESS FAR: the composed cost reached the movement");
  check_eq(mired_dist * 4, clean_dist,
           "and by the composed ratio exactly: cost 4.0 is a quarter speed");
  check(b.nav_scaled() > 0, "the cost-scaling counter moved");
  check_eq(a.nav_scaled(), 0, "[control] and it did NOT move on unmodified ground");
  std::printf("     %d ticks: clean %d, mired %d (cost 1.0 vs 4.0)\n", kWindow, clean_dist,
              mired_dist);

  // the mire EXPIRES at start_tick + duration = 40. Walk past it, then measure
  // the same window again.
  for (int t = kWindow; t < 45; ++t) sb.tick();
  check_eq(mired.service.active_fields(), 0, "the mire retired at start_tick + duration");
  const int32_t before = b.wizard(0).x;
  for (int t = 45; t < 45 + kWindow; ++t) sb.tick();
  check_eq(b.wizard(0).x - before, clean_dist,
           "after expiry the wizard covers the CLEAN distance again");
}

void t4_removal_restores_speed() {
  std::printf("4. explicit removal restores the speed too\n");
  World w;
  build(&w, /*with_mire=*/true, /*with_chasm=*/false);
  zgame::Wizards truth;
  truth.set_nav(&w.service, 16 * zgame::kOne);
  ScriptBackend be(10, 0);
  zcon::Session s(&truth, &be);
  s.set_tick_observer(&w);
  s.start(1);
  const int32_t x0 = truth.wizard(0).x;
  s.tick();
  const int32_t slow_step = truth.wizard(0).x - x0;
  check(w.service.remove_field(w.mire_id), "remove_field finds the mire");
  const int32_t x1 = truth.wizard(0).x;
  s.tick();
  const int32_t fast_step = truth.wizard(0).x - x1;
  check(fast_step > slow_step, "the step after removal is longer than the step before it");
  std::printf("     step under the mire %d, step after removal %d\n", slow_step, fast_step);
}

void t5_replay_is_exact_and_the_hook_is_load_bearing() {
  std::printf("5. a navigated session replays exactly -- and the hook is proven needed\n");
  World w;
  build(&w, /*with_mire=*/true);
  zgame::Wizards truth;
  truth.set_nav(&w.service, 16 * zgame::kOne);
  ScriptBackend be(90, 40);
  zcon::Session s(&truth, &be);
  s.set_tick_observer(&w);
  s.start(0xABCDEF);
  for (int t = 0; t < 80; ++t) s.tick();
  const std::vector<zcon::InputSnapshot> inputs = s.inputs();
  const std::vector<uint64_t> hashes = s.hashes();
  check(inputs.size() == 80, "eighty ticks recorded");

  // replay through a FRESH world, the observer attached
  World w2;
  build(&w2, /*with_mire=*/true);
  zgame::Wizards t2;
  t2.set_nav(&w2.service, 16 * zgame::kOne);
  ScriptBackend be2(90, 40);
  zcon::Session s2(&t2, &be2);
  s2.set_tick_observer(&w2);
  s2.start(0xABCDEF);
  check_eq(s2.replay_and_compare(inputs, hashes), -1,
           "the navigated recording replays with an IDENTICAL hash stream");

  // [positive control] the same replay with NO observer must DIVERGE. If it
  // did not, the hook this packet added to zcon::Session would be dead weight
  // and every assertion above would hold for a reason nobody checked.
  World w3;
  build(&w3, /*with_mire=*/true);
  zgame::Wizards t3;
  t3.set_nav(&w3.service, 16 * zgame::kOne);
  ScriptBackend be3(90, 40);
  zcon::Session s3(&t3, &be3);
  s3.start(0xABCDEF);  // NO set_tick_observer
  const int diverged = s3.replay_and_compare(inputs, hashes);
  check(diverged > 0, "[control] replaying without the tick hook DIVERGES, so the hook is real");
  std::printf("     replay with the hook: identical; without it: diverged at index %d\n",
              diverged);
}

}  // namespace

int main() {
  std::printf("wizards_nav_directed: the WIRED navigation path\n");
  t1_unnavigated_walks_into_the_hole();
  t2_navigated_stops_at_the_edge();
  t3_cost_slows_and_expiry_restores();
  t4_removal_restores_speed();
  t5_replay_is_exact_and_the_hook_is_load_bearing();
  std::printf("wizards_nav_directed: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
