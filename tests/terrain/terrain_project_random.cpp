// terrain_project_random.cpp — randomized differential for TERRAIN.PROJECT.
//
// The oracle is `project_vertex` itself (reference/src/zrender/rast.cpp), so a
// mismatch is a disagreement with the shipped pictures, not with a model.
//
// Two lanes, because one uniform lane would test almost only the second:
//
//   Lane A, LATTICE-SHAPED. A real island patch seen through a real camera:
//   vertices on a 1/32-unit grid inside a plausible envelope, a perspective
//   matrix with a camera offset that puts part of the patch behind the eye and
//   part of it far off the canvas. This is the regime the console actually
//   runs in, and it is where a wrong viewport centre or a wrong rounding is a
//   crack in a picture rather than a number.
//
//   Lane B, DOMAIN LIMIT. World coordinates over the whole ±2048 world-unit
//   envelope and matrix elements over the whole fx16 word. Nothing here is a
//   plausible camera; the point is that no input word may make the 68-bit row
//   accumulator wrap or the divider mis-rail.
//
// Each lane ASSERTS IT REACHED ITS INTERESTING STATES. A green lane that
// sampled nothing is how a flooring defect elsewhere in this tree survived
// 20,000 random triangles: lane A must see behind-the-eye vertices, guard-band
// clamps and ordinary on-canvas ones, and must NEVER saturate a row sum; lane
// B must saturate row sums and rail the divider.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_project.h"

#include "project_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"
#include "zrender/internal.hpp"

using project_test::Dev;
using project_test::mat_of;
using project_test::oracle;
using project_test::TriIn;
using project_test::TriOut;
using zhao::check;

namespace {

constexpr int32_t kOne = 1 << 16;

// Deterministic: same sequence every run, on every host. splitmix64.
struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed) {}
  uint64_t next() {
    uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
  int32_t s32() { return static_cast<int32_t>(static_cast<uint32_t>(next() >> 32)); }
  int32_t range(int32_t lo, int32_t hi) {
    const uint64_t span = static_cast<uint64_t>(hi - lo) + 1;
    return lo + static_cast<int32_t>(next() % span);
  }
};

struct Stats {
  uint32_t tris = 0;
  uint32_t behind = 0;     // vertices rejected by the near plane
  uint32_t clamped = 0;    // vertices whose screen X or Y hit the guard band
  uint32_t on_canvas = 0;  // vertices inside the viewport rectangle
  uint32_t row_sat = 0;    // vertices whose §2 row sum saturated
  uint32_t div_rail = 0;   // vertices whose ndc railed the fx16 word
};

/**
 * Classify one vertex using the RATIFIED primitives.
 *
 * This is instrumentation, never an expectation: the expectation always comes
 * from `project_vertex`. `mat4_vec4` and `fx_div_exact` are called here only to
 * answer "did this lane reach that state", which is a question the oracle's
 * return value cannot answer on its own.
 */
void classify(const zref::mat4fx& m, const zref::render::Viewport& vp, int32_t x, int32_t y,
              int32_t z, Stats& st) {
  zref::SatLedger sl{};
  const zref::vec4fx clip = zref::mat4_vec4(
      m, zref::vec4fx{zref::fx16{x}, zref::fx16{y}, zref::fx16{z}, zref::fx16{kOne}}, &sl);
  if (sl.mul != 0) ++st.row_sat;
  if (clip.w.raw <= 0) {
    ++st.behind;
    return;
  }
  zref::SatLedger dl{};
  const int32_t nx = zref::fx_div_exact(clip.x, clip.w, &dl).raw;
  const int32_t ny = zref::fx_div_exact(clip.y, clip.w, &dl).raw;
  if (nx == INT32_MAX || nx == INT32_MIN || ny == INT32_MAX || ny == INT32_MIN) ++st.div_rail;

  const zref::render::ProjOut p =
      zref::render::project_vertex(m, vp, zref::fx16{x}, zref::fx16{y}, zref::fx16{z}, nullptr);
  if (p.s.x == 524288 || p.s.x == -524288 || p.s.y == 524288 || p.s.y == -524288) ++st.clamped;
  const int32_t x0 = static_cast<int32_t>(vp.x0) << 8;
  const int32_t y0 = static_cast<int32_t>(vp.y0) << 8;
  const int32_t x1 = static_cast<int32_t>(vp.x0 + vp.w) << 8;
  const int32_t y1 = static_cast<int32_t>(vp.y0 + vp.h) << 8;
  if (p.s.x >= x0 && p.s.x < x1 && p.s.y >= y0 && p.s.y < y1) ++st.on_canvas;
}

void compare(const TriIn& in, const TriOut& got, const zref::mat4fx& m,
             const zref::render::Viewport& vp, const char* what, uint32_t* failed) {
  const TriOut want = oracle(in, m, vp);
  bool ok = got.behind == want.behind && got.src_id == want.src_id && got.view == want.view &&
            got.mat_a == want.mat_a && got.mat_b == want.mat_b && got.weight == want.weight;
  for (int k = 0; k < 3; ++k) {
    ok = ok && got.x[k] == want.x[k] && got.y[k] == want.y[k] && got.d[k] == want.d[k];
  }
  check(ok, what, static_cast<uint64_t>(static_cast<int64_t>(want.x[0])),
        static_cast<uint64_t>(static_cast<int64_t>(got.x[0])));
  if (!ok && *failed < 4) {
    ++*failed;
    std::vector<uint8_t> bytes(36, 0);
    const int32_t w[9] = {in.ax, in.ay, in.az, in.bx, in.by, in.bz, in.cx, in.cy, in.cz};
    std::memcpy(bytes.data(), w, sizeof(w));
    char e[192];
    char a[192];
    std::snprintf(e, sizeof(e), "x=%d,%d,%d y=%d,%d,%d d=%d,%d,%d behind=%u", want.x[0], want.x[1],
                  want.x[2], want.y[0], want.y[1], want.y[2], want.d[0], want.d[1], want.d[2],
                  want.behind);
    std::snprintf(a, sizeof(a), "x=%d,%d,%d y=%d,%d,%d d=%d,%d,%d behind=%u", got.x[0], got.x[1],
                  got.x[2], got.y[0], got.y[1], got.y[2], got.d[0], got.d[1], got.d[2], got.behind);
    zhao::save_failing_vector(what, bytes, e, a);
  }
}

/**
 * Lane A: a real patch through a real camera.
 *
 * The matrix is x' = f·x, y' = f·y, w = z + tz, i.e. a pinhole camera looking
 * down +Z with the eye at −tz. Varying tz sweeps the patch through the near
 * plane, which is what makes the near-plane branch a normal event and not an
 * exotic one — terrain.cpp's own comment says a near camera used to erase the
 * whole island.
 */
void lane_lattice(Dev& dev, Vzhao_terrain_project& dut, Rng& rng, int batches, Stats& st,
                  uint32_t* failed) {
  // The two Duo view blocks, stacked (video_rules §3.1) — the real pair, so the
  // view select is exercised by the lane that models the real machine.
  const zref::render::Viewport vps[2] = {{0, 0, 256, 192}, {0, 192, 256, 192}};
  for (int b = 0; b < batches; ++b) {
    const int view = static_cast<int>(rng.next() & 1);
    const zref::render::Viewport vp = vps[view];
    const int32_t f = rng.range(kOne / 2, 3 * kOne);
    // tz is the eye's distance in front of the patch. Sweeping it down through
    // zero walks the patch through the near plane, which is what makes the
    // near-plane branch and the guard band ORDINARY events in this lane rather
    // than exotic ones — terrain.cpp's own note says a near camera used to
    // erase the whole island.
    const int32_t tz = rng.range(-2 * kOne, 20 * kOne);
    const int32_t mm[16] = {f, 0, 0, 0, 0, f, 0, 0, 0, 0, kOne, 0, 0, 0, kOne, tz};
    const zref::mat4fx m = mat_of(mm);
    dev.configure(view, m, vp);

    // A 1/32-unit lattice cell, heights within ±1 unit: what a 32×32-cell
    // Mantle patch emits (terrain_rules §2, sub-metre by design).
    const int32_t step = kOne / 32;
    std::vector<TriIn> batch;
    for (int i = 0; i < 24; ++i) {
      // A 32x32-cell patch's worth of lattice offsets around the origin.
      const int32_t ox = rng.range(-512, 512) * step;
      const int32_t oz = rng.range(-512, 512) * step;
      TriIn t;
      t.ax = ox;
      t.az = oz;
      t.ay = rng.range(-kOne, kOne);
      t.bx = ox + step;
      t.bz = oz;
      t.by = rng.range(-kOne, kOne);
      t.cx = ox;
      t.cz = oz + step;
      t.cy = rng.range(-kOne, kOne);
      t.src_id = static_cast<uint16_t>(rng.next() & 0xFFFF);
      t.view = static_cast<uint8_t>(view);
      t.mat_a = static_cast<uint8_t>(rng.next() & 0xFF);
      t.mat_b = static_cast<uint8_t>(rng.next() & 0xFF);
      t.weight = static_cast<uint8_t>(rng.next() & 0xFF);
      batch.push_back(t);
      classify(m, vp, t.ax, t.ay, t.az, st);
      classify(m, vp, t.bx, t.by, t.bz, st);
      classify(m, vp, t.cx, t.cy, t.cz, st);
      ++st.tris;
    }
    const uint32_t mask = static_cast<uint32_t>(rng.next());
    const std::vector<TriOut> got = dev.run(batch, mask);
    check(got.size() == batch.size(), "lane A: every packet comes back", batch.size(), got.size());
    for (size_t i = 0; i < got.size() && i < batch.size(); ++i) {
      compare(batch[i], got[i], m, vp, "terrain_project_laneA", failed);
    }
    check(dut.idle_o == 1, "lane A: idle after the batch drains", 1, dut.idle_o);
  }
}

/** Lane B: the domain limit — the whole world envelope, the whole matrix word. */
void lane_limit(Dev& dev, Vzhao_terrain_project& dut, Rng& rng, int batches, Stats& st,
                uint32_t* failed) {
  // ±2048 world units is spec/qformats.md §8's guard-band envelope expressed in
  // world space; the coordinate word itself goes wider, and both are sampled.
  const int32_t kEnv = 2048 * kOne;
  for (int b = 0; b < batches; ++b) {
    const zref::render::Viewport vp{
        static_cast<uint32_t>(rng.range(0, 128)), static_cast<uint32_t>(rng.range(0, 128)),
        static_cast<uint32_t>(rng.range(1, 512)), static_cast<uint32_t>(rng.range(1, 512))};
    int32_t mm[16];
    for (int i = 0; i < 16; ++i) mm[i] = ((rng.next() & 3) == 0) ? rng.s32() : rng.range(-4, 4);
    const zref::mat4fx m = mat_of(mm);
    dev.configure(0, m, vp);

    std::vector<TriIn> batch;
    for (int i = 0; i < 24; ++i) {
      TriIn t;
      const bool word = ((rng.next() & 1) != 0);
      t.ax = word ? rng.s32() : rng.range(-kEnv, kEnv);
      t.ay = word ? rng.s32() : rng.range(-kEnv, kEnv);
      t.az = word ? rng.s32() : rng.range(-kEnv, kEnv);
      t.bx = word ? rng.s32() : rng.range(-kEnv, kEnv);
      t.by = word ? rng.s32() : rng.range(-kEnv, kEnv);
      t.bz = word ? rng.s32() : rng.range(-kEnv, kEnv);
      t.cx = word ? rng.s32() : rng.range(-kEnv, kEnv);
      t.cy = word ? rng.s32() : rng.range(-kEnv, kEnv);
      t.cz = word ? rng.s32() : rng.range(-kEnv, kEnv);
      t.src_id = static_cast<uint16_t>(rng.next() & 0xFFFF);
      t.view = static_cast<uint8_t>(0);
      batch.push_back(t);
      classify(m, vp, t.ax, t.ay, t.az, st);
      classify(m, vp, t.bx, t.by, t.bz, st);
      classify(m, vp, t.cx, t.cy, t.cz, st);
      ++st.tris;
    }
    const uint32_t mask = static_cast<uint32_t>(rng.next());
    const std::vector<TriOut> got = dev.run(batch, mask);
    check(got.size() == batch.size(), "lane B: every packet comes back", batch.size(), got.size());
    for (size_t i = 0; i < got.size() && i < batch.size(); ++i) {
      compare(batch[i], got[i], m, vp, "terrain_project_laneB", failed);
    }
    check(dut.idle_o == 1, "lane B: idle after the batch drains", 1, dut.idle_o);
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  bool nightly = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;
  }
  const int batches = nightly ? 400 : 40;

  Vzhao_terrain_project dut;
  Dev dev(dut);
  dev.reset();

  Rng rng(0x7A0E'C051ULL);
  Stats a{};
  Stats b{};
  uint32_t failed_a = 0;
  uint32_t failed_b = 0;
  lane_lattice(dev, dut, rng, batches, a, &failed_a);
  dev.reset();
  lane_limit(dev, dut, rng, batches, b, &failed_b);

  std::printf(
      "[terrain_project_random] lane A: %u tris, behind %u, clamped %u, on-canvas %u, "
      "row-sat %u, div-rail %u\n",
      a.tris, a.behind, a.clamped, a.on_canvas, a.row_sat, a.div_rail);
  std::printf(
      "[terrain_project_random] lane B: %u tris, behind %u, clamped %u, on-canvas %u, "
      "row-sat %u, div-rail %u\n",
      b.tris, b.behind, b.clamped, b.on_canvas, b.row_sat, b.div_rail);

  // ---- each lane must have reached the states it exists to reach -----------
  check(a.behind > 32, "lane A reached the near plane", 32, a.behind);
  check(a.clamped > 32, "lane A reached the guard band", 32, a.clamped);
  check(a.on_canvas > 32, "lane A drew inside the viewport", 32, a.on_canvas);
  check(a.row_sat == 0, "lane A never saturates a row sum (it is the real regime)", 0, a.row_sat);

  check(b.row_sat > 32, "lane B saturated a row sum", 32, b.row_sat);
  check(b.div_rail > 32, "lane B railed the divider", 32, b.div_rail);
  check(b.clamped > 32, "lane B reached the guard band", 32, b.clamped);
  check(b.behind > 32, "lane B reached the near plane", 32, b.behind);
  check(b.on_canvas > 32, "lane B still lands on canvas sometimes", 32, b.on_canvas);

  return zhao::report_and_exit("terrain_project_random");
}
