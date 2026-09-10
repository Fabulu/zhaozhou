// geom_cull_mutant_control.cpp — THE POSITIVE CONTROL for the cull
// differential's checker, driving a committed MUTANT.
//
// geom_cull_directed asserts bit-identity of every verdict against zref::cull,
// and on the MUL_LANES=2 (default) and MUL_LANES=1 arms that claim leans on the
// issue/commit accumulator starting a FRESH sum at every plane boundary —
// machinery the MUL_LANES=4 spatial arm never had, so no earlier run of the
// suite has demonstrated the checker can see a boundary fault.
// tests/mutants/zhao_geom_cull_mutant.sv removes the clear; THIS TEST PASSES
// WHEN THE DIFFERENTIAL FAILS — inverse polarity, evidence about the
// instrument, not about the design.
//
// Two things are asserted about the fault's shape, because both are lessons:
//
//   · the exact walk law STILL HOLDS on the mutant. Every evaluation completes
//     in precisely the declared number of ticks and ready_o returns on cue. A
//     test that pinned latency and trusted the verdict would report this block
//     clean. Counters and timing pins see how many times and how long; only
//     the differential sees WHAT.
//   · the fault is a BOUNDARY fault, not an arithmetic one: the very first
//     plane test after reset is formed from an empty accumulator and is
//     correct. So a sphere that the FIRST plane (left, view 0) alone rejects
//     is still rejected correctly, while spheres inside the frustum — whose
//     verdict depends on every later plane being clean — get contaminated.

#include <cmath>
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_geom_cull_mutant.h"

#include "zhao_sim.hpp"
#include "zref/zref_cull.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_trig.hpp"

namespace {

using zhao::check;
using zref::fx16;
using zref::mat4fx;
using zref::vec3fx;
namespace zc = zref::cull;

constexpr int32_t ONE = 1 << 16;

// The default arm's walk; the mutant is built at the default MUL_LANES.
constexpr int kWalk = 21;

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed) {}
  uint64_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
  }
  int32_t range(int32_t lo, int32_t hi) {
    return lo +
           static_cast<int32_t>(next() % static_cast<uint64_t>(static_cast<int64_t>(hi) - lo + 1));
  }
};

/** The directed suite's perspective camera, without the shear (not needed here). */
mat4fx make_vp(double fov_deg, double aspect, double eye_z) {
  const double f = 1.0 / std::tan(fov_deg * 3.14159265358979 / 360.0);
  mat4fx m{};
  auto set = [&](int i, int j, double v) { m.m[i][j] = fx16{static_cast<int32_t>(v * 65536.0)}; };
  set(0, 0, f / aspect);
  set(1, 1, f);
  set(2, 2, 1.0);
  set(3, 2, -1.0);
  set(3, 3, eye_z);
  return m;
}

void write_view(Vzhao_geom_cull_mutant& dut, int view, const mat4fx& m) {
  for (int k = 0; k < 16; ++k) {
    dut.cfg_we_i = 1;
    dut.cfg_view_i = static_cast<uint8_t>(view);
    dut.cfg_addr_i = static_cast<uint8_t>(k);
    dut.cfg_data_i = static_cast<uint32_t>(m.m[k / 4][k % 4].raw);
    zhao::tick(dut);
  }
  dut.cfg_we_i = 0;
  dut.cfg_addr_i = 0;
  dut.cfg_data_i = 0;
  dut.eval();
}

int wait_ready(Vzhao_geom_cull_mutant& dut) {
  int n = 0;
  while (!dut.ready_o && n < 4000) {
    zhao::tick(dut);
    ++n;
  }
  check(dut.ready_o != 0, "mutant: block became ready after the matrix writes", 1, dut.ready_o);
  return n;
}

/** One instance through the mutant; returns the verdict and asserts the walk. */
zc::Verdict dut_cull(Vzhao_geom_cull_mutant& dut, uint8_t active, vec3fx c, fx16 r) {
  dut.active_i = active;
  dut.centre_x_i = c.x.raw;
  dut.centre_y_i = c.y.raw;
  dut.centre_z_i = c.z.raw;
  dut.radius_i = r.raw;
  dut.tick_i = 1;
  zhao::tick(dut);
  dut.tick_i = 0;
  dut.eval();
  int n = 0;
  while (!dut.valid_o && n < 64) {
    zhao::tick(dut);
    ++n;
  }
  check(dut.valid_o == 1, "mutant: every evaluation still completes", 1, dut.valid_o);
  check(n == kWalk, "mutant: the exact walk law STILL HOLDS (the pin cannot see this fault)",
        kWalk, static_cast<uint64_t>(n));
  check(dut.ready_o == 1, "mutant: ready_o still returns with the verdict", 1, dut.ready_o);
  zc::Verdict v{};
  v.visible_mask = static_cast<uint8_t>(dut.vis_o);
  v.reject = dut.reject_o != 0;
  return v;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_geom_cull_mutant dut;
  dut.rst_n = 0;
  dut.cfg_we_i = 0;
  dut.cfg_view_i = 0;
  dut.cfg_addr_i = 0;
  dut.cfg_data_i = 0;
  dut.tick_i = 0;
  dut.active_i = 0;
  dut.centre_x_i = 0;
  dut.centre_y_i = 0;
  dut.centre_z_i = 0;
  dut.radius_i = 0;
  dut.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);

  const mat4fx m0 = make_vp(60.0, 4.0 / 3.0, 40.0);
  const mat4fx m1 = make_vp(90.0, 1.0, 12.0);
  write_view(dut, 0, m0);
  write_view(dut, 1, m1);
  wait_ready(dut);
  zc::View views[2] = {zc::make_view(m0), zc::make_view(m1)};

  // ---- the boundary-fault signature ----------------------------------------
  // First instance after the matrices land: a sphere far to the LEFT of view 0
  // with only view 0 active. Plane 0 (left) alone rejects it, and plane 0 of
  // the first instance is the one plane test the mutant forms correctly — so
  // this verdict must still match. (Every later plane in the same instance is
  // contaminated, but an "outside" that is already true cannot be made false
  // by more "outside" bits.)
  {
    const vec3fx far_left{fx16{-500 * ONE}, fx16{0}, fx16{0}};
    const zc::Verdict want = zc::cull_instance(views, 0x1, far_left, fx16{ONE});
    const zc::Verdict got = dut_cull(dut, 0x1, far_left, fx16{ONE});
    check(want.reject, "signature: the oracle rejects the far-left sphere on plane 0", 1,
          want.reject);
    check(got.reject == want.reject && got.visible_mask == want.visible_mask,
          "signature: a verdict decided by the FIRST plane is still right", 1,
          (got.reject == want.reject && got.visible_mask == want.visible_mask) ? 1 : 0);
  }

  // ---- the differential, on spheres the oracle sees INSIDE ------------------
  // These are the verdicts that need every plane clean. Random centres in the
  // directed suite's own range; count how many the mutant gets wrong.
  Prng rng(0xC0117EEDu);
  int cases = 0, mismatches = 0, inside_cases = 0, inside_mismatches = 0;
  for (int i = 0; i < 400; ++i) {
    const vec3fx c{fx16{rng.range(-60 * ONE, 60 * ONE)}, fx16{rng.range(-60 * ONE, 60 * ONE)},
                   fx16{rng.range(-60 * ONE, 60 * ONE)}};
    const fx16 r{rng.range(0, 8 * ONE)};
    const zc::Verdict want = zc::cull_instance(views, 0x3, c, r);
    const zc::Verdict got = dut_cull(dut, 0x3, c, r);
    const bool bad = (got.reject != want.reject) || (got.visible_mask != want.visible_mask);
    ++cases;
    if (bad) ++mismatches;
    if (!want.reject) {
      ++inside_cases;
      if (bad) ++inside_mismatches;
    }
  }
  check(inside_cases > 0, "the sample contains spheres the oracle keeps", 1,
        inside_cases > 0 ? 1 : 0);
  // INVERTED POLARITY — this control passes only when the differential checker
  // would FAIL the mutant.
  check(mismatches > 0, "the differential checker FIRES on the uncleaned accumulator", 1,
        mismatches > 0 ? 1 : 0);
  std::printf("[info] boundary mutant: %d of %d verdicts diverge (%d of %d oracle-visible spheres)\n",
              mismatches, cases, inside_mismatches, inside_cases);

  dut.final();
  return zhao::report_and_exit("geom_cull_mutant_control");
}
