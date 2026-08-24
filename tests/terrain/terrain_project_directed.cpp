// terrain_project_directed.cpp — TERRAIN.PROJECT against `project_vertex`.
//
// THE ORACLE IS THE SHIPPED FUNCTION, not a model of it. `project_vertex` in
// reference/src/zrender/rast.cpp is what every golden capture in this tree was
// drawn with, and this test calls it — no reimplementation exists anywhere in
// this lane. So a mismatch here means the RTL disagrees with the pictures the
// project already ships.
//
// The cases are chosen around the four things this arithmetic gets wrong when
// it is wrong:
//   · the near plane (`clip.w <= 0` is a REJECTION, and it carries zeros),
//   · the exact division (round-half-up, both signs, and both rails),
//   · the single-rounding fx_mad (a second rounding here is invisible until
//     a seam cracks), and
//   · the guard band, which is a CLAMP and not a clip.

#include <cstdint>
#include <cstdio>
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
using project_test::persp;
using project_test::TriIn;
using project_test::TriOut;
using zhao::check;

namespace {

constexpr int32_t kOne = 1 << 16;  // 1.0 in fx16

const zref::render::Viewport kCanvas{0, 0, 256, 192};

/** Compare one collected packet against `project_vertex` field for field. */
void expect_one(const TriIn& in, const TriOut& got, const zref::mat4fx& m,
                const zref::render::Viewport& vp, const char* what) {
  const TriOut want = oracle(in, m, vp);
  for (int k = 0; k < 3; ++k) {
    check(got.x[k] == want.x[k], what, static_cast<uint64_t>(static_cast<int64_t>(want.x[k])),
          static_cast<uint64_t>(static_cast<int64_t>(got.x[k])));
    check(got.y[k] == want.y[k], what, static_cast<uint64_t>(static_cast<int64_t>(want.y[k])),
          static_cast<uint64_t>(static_cast<int64_t>(got.y[k])));
    check(got.d[k] == want.d[k], what, static_cast<uint64_t>(static_cast<int64_t>(want.d[k])),
          static_cast<uint64_t>(static_cast<int64_t>(got.d[k])));
  }
  check(got.behind == want.behind, what, want.behind, got.behind);
  check(got.src_id == want.src_id, what, want.src_id, got.src_id);
}

/** Push one triangle with one configured view and check it. */
void expect(Dev& dev, const TriIn& in, const zref::mat4fx& m, const zref::render::Viewport& vp,
            const char* what) {
  const std::vector<TriOut> got = dev.run({in});
  check(got.size() == 1, what, 1, got.size());
  if (got.size() == 1) expect_one(in, got[0], m, vp, what);
}

TriIn tri(int32_t ax, int32_t ay, int32_t az, int32_t bx, int32_t by, int32_t bz, int32_t cx,
          int32_t cy, int32_t cz, uint16_t src) {
  TriIn t;
  t.ax = ax;
  t.ay = ay;
  t.az = az;
  t.bx = bx;
  t.by = by;
  t.bz = bz;
  t.cx = cx;
  t.cy = cy;
  t.cz = cz;
  t.src_id = src;
  return t;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_project dut;
  Dev dev(dut);
  dev.reset();

  uint16_t src = 1;

  // =========================================================================
  // 1. the orthographic fixture, with two values computed BY HAND
  // =========================================================================
  // x' = x, y' = y, w = 1.0. Then ndc = world, and with a 256x192 canvas
  //   screen_fx = ndc·128 + 128·65536      (hw = 256·2^15, cx = 128 << 16)
  //   px        = (screen_fx + 128) >> 8
  // so world (0,0) lands on 32768 = 128.0 px and world x = 1.0 lands on
  // 65536 = 256.0 px — the right-hand edge of the viewport, because ndc +1 is
  // the edge. Getting the half-extent or the centre wrong moves both.
  {
    const int32_t mo[16] = {kOne, 0, 0, 0, 0, kOne, 0, 0, 0, 0, kOne, 0, 0, 0, 0, kOne};
    const zref::mat4fx m = mat_of(mo);
    dev.configure(0, m, kCanvas);

    const TriIn t = tri(0, 0, 0, kOne, 0, 0, 0, kOne, 0, src++);
    const std::vector<TriOut> got = dev.run({t});
    check(got.size() == 1, "ortho: one triangle in, one out", 1, got.size());
    if (got.size() == 1) {
      expect_one(t, got[0], m, kCanvas, "ortho fixture matches project_vertex");
      check(got[0].x[0] == 32768, "world origin lands on the canvas centre column", 32768,
            static_cast<uint64_t>(got[0].x[0]));
      check(got[0].y[0] == 24576, "world origin lands on the canvas centre row", 24576,
            static_cast<uint64_t>(got[0].y[0]));
      check(got[0].x[1] == 65536, "ndc +1 lands on the viewport's right edge", 65536,
            static_cast<uint64_t>(got[0].x[1]));
      check(got[0].behind == 0, "w = 1.0 is in front of the eye", 0, got[0].behind);
      check(got[0].d[0] == kOne, "w = 1.0 gives depth 1/w = 1.0", kOne,
            static_cast<uint64_t>(got[0].d[0]));
    }

    // A viewport that is not at the origin must move the whole picture.
    const zref::render::Viewport vp_off{64, 32, 128, 96};
    dev.configure(0, m, vp_off);
    expect(dev, tri(0, 0, 0, kOne, 0, 0, 0, -kOne, 0, src++), m, vp_off,
           "an offset viewport moves the picture");
  }

  // =========================================================================
  // 2. the perspective fixture at several depths
  // =========================================================================
  {
    const zref::mat4fx m = persp(kOne, kOne);
    dev.configure(0, m, kCanvas);
    const int32_t depths[6] = {kOne / 4, kOne / 2, kOne, 2 * kOne, 8 * kOne, 64 * kOne};
    for (int i = 0; i < 6; ++i) {
      const int32_t z = depths[i];
      expect(dev, tri(kOne, kOne, z, -kOne, kOne, z, 0, -kOne, z, src++), m, kCanvas,
             "perspective at depth");
      // 1/w must shrink as the vertex recedes — the D7 depth lane's whole
      // point (larger is closer), checked as an ORDER, not just a value.
      if (i > 0) {
        const std::vector<TriOut> a = dev.run(
            {tri(kOne, kOne, depths[i - 1], 0, 0, depths[i - 1], 0, 0, depths[i - 1], src++)});
        const std::vector<TriOut> b = dev.run({tri(kOne, kOne, z, 0, 0, z, 0, 0, z, src++)});
        if (a.size() == 1 && b.size() == 1) {
          check(a[0].d[0] > b[0].d[0], "1/w decreases with depth", static_cast<uint64_t>(a[0].d[0]),
                static_cast<uint64_t>(b[0].d[0]));
        }
      }
    }
  }

  // =========================================================================
  // 3. the near plane: a whole-primitive REJECTION that carries zeros
  // =========================================================================
  // `project_vertex` returns a default-constructed ProjOut when clip.w <= 0,
  // so the vertex carries {0,0,0}. This block must too, per vertex and not per
  // triangle: the mask has three independent bits and GEOM.CLIP reads all
  // three. Both boundary values are here — w == 0 is REJECTED (the test is
  // `<= 0`, not `< 0`), and w == 1 raw is accepted.
  {
    const zref::mat4fx m = persp(kOne, kOne);
    dev.configure(0, m, kCanvas);

    struct Case {
      int32_t za, zb, zc;
      uint8_t mask;
      const char* what;
    };
    const Case cases[8] = {
        {kOne, kOne, kOne, 0, "all three in front"},
        {0, kOne, kOne, 1, "w == 0 rejects vertex A"},
        {kOne, 0, kOne, 2, "w == 0 rejects vertex B"},
        {kOne, kOne, 0, 4, "w == 0 rejects vertex C"},
        {-kOne, kOne, kOne, 1, "w < 0 rejects vertex A"},
        {-kOne, -kOne, kOne, 3, "two behind"},
        {-1, -1, -1, 7, "all three behind"},
        {1, 1, 1, 0, "w == 1 raw is in front"},
    };
    for (int i = 0; i < 8; ++i) {
      const TriIn t =
          tri(kOne, kOne, cases[i].za, -kOne, kOne, cases[i].zb, 0, -kOne, cases[i].zc, src++);
      const std::vector<TriOut> got = dev.run({t});
      check(got.size() == 1, cases[i].what, 1, got.size());
      if (got.size() != 1) continue;
      expect_one(t, got[0], m, kCanvas, cases[i].what);
      check(got[0].behind == cases[i].mask, cases[i].what, cases[i].mask, got[0].behind);
      for (int k = 0; k < 3; ++k) {
        if (((cases[i].mask >> k) & 1) != 0) {
          check(got[0].x[k] == 0 && got[0].y[k] == 0 && got[0].d[k] == 0,
                "a behind-the-eye vertex carries zeros", 0,
                static_cast<uint64_t>(got[0].x[k] | got[0].y[k] | got[0].d[k]));
        }
      }
    }
  }

  // =========================================================================
  // 4. the exact division, at the round-half-up boundary in both directions
  // =========================================================================
  // m00 = m11 = 1 raw and m33 = 2.0 give clip.{x,y} = world raw >> 16 and
  // clip.w = 131072, so ndc = round_half_up(clip / 2). At clip = +1 the true
  // quotient is +0.5 and rounds to 1; at clip = -1 it is -0.5 and rounds to 0,
  // NOT to -1. Round-half-up is not round-away-from-zero, and a divider that
  // negates-then-rounds gets the negative side wrong by one.
  {
    const int32_t mb[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, kOne, 0, 0, 0, 0, 2 * kOne};
    const zref::mat4fx m = mat_of(mb);
    dev.configure(0, m, kCanvas);

    struct HalfCase {
      int32_t wx, wy;
      int32_t ndc_x, ndc_y;
      const char* what;
    };
    const HalfCase hc[6] = {
        {kOne, kOne, 1, 1, "+0.5 rounds up"},
        {-kOne, -kOne, 0, 0, "-0.5 rounds to zero, not away"},
        {3 * kOne, 3 * kOne, 2, 2, "+1.5 rounds up"},
        {-3 * kOne, -3 * kOne, -1, -1, "-1.5 rounds up (toward zero)"},
        {2 * kOne, 2 * kOne, 1, 1, "+1.0 is exact"},
        {-2 * kOne, -2 * kOne, -1, -1, "-1.0 is exact"},
    };
    for (int i = 0; i < 6; ++i) {
      // Confirm the hand-derived quotient IS what the ratified primitive says,
      // so the case below is anchored to §3 and not to this test's arithmetic.
      const int32_t q =
          zref::fx_div_exact(zref::fx16{hc[i].wx >> 16}, zref::fx16{2 * kOne}, nullptr).raw;
      check(q == hc[i].ndc_x, hc[i].what, static_cast<uint64_t>(hc[i].ndc_x),
            static_cast<uint64_t>(q));
      expect(dev, tri(hc[i].wx, hc[i].wy, kOne, 0, 0, kOne, kOne, kOne, kOne, src++), m, kCanvas,
             hc[i].what);
    }
  }

  // =========================================================================
  // 5. the divider's rails, and the negative value that is NOT a rail
  // =========================================================================
  // m33 = 1 raw makes clip.w = 1, so ndc = clip << 16 exactly and the fx16
  // word is reachable by hand. |clip| = 32768 gives exactly -2^31, which IS
  // INT32_MIN and is NOT a saturation; |clip| = 32769 saturates to the same
  // word. Both must come out as INT32_MIN, which is why the rail test can be
  // taken before the division without losing the exact case.
  {
    const int32_t mr[16] = {kOne, 0, 0, 0, 0, kOne, 0, 0, 0, 0, kOne, 0, 0, 0, 0, 1};
    const zref::mat4fx m = mat_of(mr);
    dev.configure(0, m, kCanvas);

    const int32_t clips[8] = {32767, 32768, 32769, 65536, -32767, -32768, -32769, -65536};
    for (int i = 0; i < 8; ++i) {
      // m00 = 1.0, so clip.x = rescale(65536 · world, 16) = world exactly.
      const int32_t w = clips[i];
      expect(dev, tri(w, w, kOne, 0, 0, kOne, 0, 0, kOne, src++), m, kCanvas,
             "the divider rails and the exact INT32_MIN");
    }
    // And by hand: 1/w with w = 1 raw is 2^32, which saturates to INT32_MAX.
    const std::vector<TriOut> got = dev.run({tri(0, 0, kOne, 0, 0, kOne, 0, 0, kOne, src++)});
    if (got.size() == 1) {
      check(got[0].d[0] == 0x7FFFFFFF, "1/w saturates at the fx16 rail", 0x7FFFFFFFu,
            static_cast<uint32_t>(got[0].d[0]));
    }
  }

  // =========================================================================
  // 6. the guard band is a CLAMP
  // =========================================================================
  // §8: convert to S 12.8, THEN clamp to ±2048 px = ±524,288 subpixels.
  // GEOM.CLIP's header names this block as the enforcer of exactly that, and
  // its 21-bit port cannot represent anything wider, so a missing clamp here
  // is a silent truncation there.
  {
    const zref::mat4fx m = persp(kOne, kOne);
    dev.configure(0, m, kCanvas);
    const int32_t far_ = 30 * kOne;
    const TriIn t = tri(far_, far_, kOne, -far_, -far_, kOne, far_, -far_, kOne, src++);
    const std::vector<TriOut> got = dev.run({t});
    check(got.size() == 1, "guard band: one triangle out", 1, got.size());
    if (got.size() == 1) {
      expect_one(t, got[0], m, kCanvas, "guard band matches to_screen_xy");
      check(got[0].x[0] == 524288, "+X clamps to the guard band", 524288,
            static_cast<uint64_t>(got[0].x[0]));
      check(got[0].x[1] == -524288, "-X clamps to the guard band",
            static_cast<uint64_t>(static_cast<int64_t>(-524288)),
            static_cast<uint64_t>(static_cast<int64_t>(got[0].x[1])));
      check(got[0].y[0] == 524288, "+Y clamps to the guard band", 524288,
            static_cast<uint64_t>(got[0].y[0]));
      check(got[0].y[1] == -524288, "-Y clamps to the guard band",
            static_cast<uint64_t>(static_cast<int64_t>(-524288)),
            static_cast<uint64_t>(static_cast<int64_t>(got[0].y[1])));
    }
  }

  // =========================================================================
  // 7. an odd viewport, which is the only way the fx_mad rounding shows
  // =========================================================================
  // With an even width the fx_mad product's low 16 bits are always zero and
  // the §3 rounding is invisible. Width 3 makes hw = 3·2^15, whose low bits
  // put an odd ndc exactly on the half — so a double rounding, or a truncation
  // instead of round-half-up, changes the answer here and nowhere else.
  {
    const int32_t mb[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, kOne, 0, 0, 0, 0, 3};
    const zref::mat4fx m = mat_of(mb);
    const zref::render::Viewport odd_vp{1, 1, 3, 5};
    dev.configure(0, m, odd_vp);

    int halves = 0;
    for (int32_t c = -12; c <= 12; ++c) {
      const int32_t w = c * kOne;  // clip.x = c
      const TriIn t = tri(w, w, kOne, w + kOne, w, kOne, w, w + kOne, kOne, src++);
      const std::vector<TriOut> got = dev.run({t});
      check(got.size() == 1, "odd viewport sweep", 1, got.size());
      if (got.size() == 1) expect_one(t, got[0], m, odd_vp, "odd viewport sweep");
      // Instrumentation only (the ratified primitives, used to CLASSIFY the
      // case, never to produce the expectation): did this vertex actually sit
      // on the fx_mad half?
      const int32_t ndc = zref::fx_div_exact(zref::fx16{c}, zref::fx16{3}, nullptr).raw;
      const int64_t prod = static_cast<int64_t>(ndc) * (3 << 15);
      if ((prod & 0xFFFF) == 0x8000) ++halves;
    }
    check(halves > 0, "the odd-viewport sweep reached the fx_mad half", 1,
          static_cast<uint64_t>(halves));
  }

  // =========================================================================
  // 8. a row sum that saturates the fx16 word
  // =========================================================================
  // §2 saturates each row after ONE rescale. A matrix whose row sum leaves the
  // word must clamp, not wrap — and the sum is formed at 68 bits precisely so
  // that the clamp is a decision and not an accident of a narrow accumulator.
  {
    const int32_t big = 0x7FFFFFFF;
    const int32_t ms[16] = {big, big, big, big, big, big, big, big, 0, 0, kOne, 0, 0, 0, 0, kOne};
    const zref::mat4fx m = mat_of(ms);
    dev.configure(0, m, kCanvas);
    expect(dev, tri(big, big, big, -big, -big, -big, big, -big, big, src++), m, kCanvas,
           "a saturating row sum clamps, it does not wrap");
    expect(dev, tri(INT32_MIN, INT32_MIN, INT32_MIN, 1, -1, 1, 0, 0, 0, src++), m, kCanvas,
           "INT32_MIN world coordinates clamp too");
  }

  // =========================================================================
  // 9. the dual view: two register sets, one packet bit
  // =========================================================================
  {
    const zref::mat4fx m0 = persp(kOne, kOne);
    const zref::mat4fx m1 = persp(2 * kOne, 4 * kOne);
    const zref::render::Viewport vp0{0, 0, 256, 192};
    const zref::render::Viewport vp1{0, 192, 256, 192};  // video_rules §3.1 stacked Duo
    dev.configure(0, m0, vp0);
    dev.configure(1, m1, vp1);

    std::vector<TriIn> batch;
    for (int i = 0; i < 8; ++i) {
      TriIn t = tri(kOne / 2, kOne / 3, kOne + i * 4096, -kOne / 2, kOne / 5, kOne + i * 4096,
                    kOne / 7, -kOne / 2, kOne + i * 4096, src++);
      t.view = static_cast<uint8_t>(i & 1);
      t.mat_a = static_cast<uint8_t>(0x10 + i);
      t.mat_b = static_cast<uint8_t>(0xA0 + i);
      t.weight = static_cast<uint8_t>(i * 31);
      batch.push_back(t);
    }
    const std::vector<TriOut> got = dev.run(batch);
    check(got.size() == batch.size(), "dual view: every packet comes back", batch.size(),
          got.size());
    for (size_t i = 0; i < got.size(); ++i) {
      const bool v1 = (batch[i].view != 0);
      expect_one(batch[i], got[i], v1 ? m1 : m0, v1 ? vp1 : vp0, "each packet uses its own view");
      check(got[i].view == batch[i].view, "the view tag rides the packet", batch[i].view,
            got[i].view);
      check(got[i].mat_a == batch[i].mat_a && got[i].mat_b == batch[i].mat_b &&
                got[i].weight == batch[i].weight,
            "the mosaic candidates ride through unaltered", batch[i].mat_a, got[i].mat_a);
    }
    // The two views must actually DIFFER, or the check above proves nothing.
    if (got.size() >= 2) {
      check(got[0].x[0] != got[1].x[0] || got[0].y[0] != got[1].y[0],
            "the two views really do project differently", static_cast<uint64_t>(got[0].x[0]),
            static_cast<uint64_t>(got[1].x[0]));
    }
  }

  // =========================================================================
  // 10. backpressure, order, and the counter
  // =========================================================================
  {
    const zref::mat4fx m = persp(kOne, kOne);
    dev.configure(0, m, kCanvas);

    std::vector<TriIn> batch;
    for (int i = 0; i < 24; ++i) {
      batch.push_back(tri(kOne / 2 - i * 997, kOne / 3 + i * 1013, kOne + i * 271,
                          -kOne / 2 + i * 811, kOne / 5 - i * 601, kOne + i * 271,
                          kOne / 7 + i * 409, -kOne / 2 - i * 307, kOne + i * 271,
                          static_cast<uint16_t>(0x300 + i)));
    }

    const uint32_t masks[4] = {0u, 0xAAAAAAAAu, 0xFFFFFFFEu, 0x0F0F0F0Fu};
    const char* mask_what[4] = {"no stall", "alternate stall", "stalled 31 cycles in 32",
                                "burst stall"};
    for (int mi = 0; mi < 4; ++mi) {
      dev.reset();
      dev.configure(0, m, kCanvas);
      int idle_bad = 0;
      const std::vector<TriOut> got = dev.run(batch, masks[mi], nullptr, &idle_bad);
      check(got.size() == batch.size(), mask_what[mi], batch.size(), got.size());
      // THE OTHER DIRECTION OF THE idle_o CLAIM, and it did not exist before
      // 2026-08-24. `idle_o == 1 after the drain` is checked below and is also
      // checked by both random lanes -- and all three pass against
      // `assign idle_o = 1'b1`. Mutant M20 (the core's output register dropped
      // from `busy_o`, so idle asserts one cycle early) survived the entire
      // suite on that gap. `idle_bad` counts cycles where idle_o was HIGH while
      // work was outstanding, which is a state the rigid pipeline cannot be in.
      check(idle_bad == 0, "idle_o is LOW on every cycle work is in flight", 0,
            static_cast<uint64_t>(idle_bad));
      for (size_t i = 0; i < got.size() && i < batch.size(); ++i) {
        expect_one(batch[i], got[i], m, kCanvas, mask_what[mi]);
        check(got[i].src_id == batch[i].src_id, "packets keep their order under stall",
              batch[i].src_id, got[i].src_id);
      }
      check(dut.terrain_triangles_emitted_o == batch.size(), "the counter counts triangles",
            batch.size(), dut.terrain_triangles_emitted_o);
      check(dut.idle_o == 1, "the block is idle once the stream has drained", 1, dut.idle_o);
    }
  }

  // =========================================================================
  // 11. the measured rate, printed rather than claimed
  // =========================================================================
  {
    const zref::mat4fx m = persp(kOne, kOne);
    dev.reset();
    dev.configure(0, m, kCanvas);
    std::vector<TriIn> batch;
    for (int i = 0; i < 128; ++i) {
      batch.push_back(tri(kOne / 2, kOne / 3, kOne + i, -kOne / 2, kOne / 5, kOne + i, kOne / 7,
                          -kOne / 2, kOne + i, static_cast<uint16_t>(i)));
    }
    int cycles = 0;
    const std::vector<TriOut> got = dev.run(batch, 0, &cycles);
    check(got.size() == batch.size(), "rate measurement completes", batch.size(), got.size());
    std::printf(
        "[terrain_project] %d triangles in %d cycles = %.2f cycles/triangle "
        "(%.2f vertices/clock)\n",
        static_cast<int>(got.size()), cycles,
        static_cast<double>(cycles) / static_cast<double>(got.size()),
        3.0 * static_cast<double>(got.size()) / static_cast<double>(cycles));
    // The steady state is three clocks per triangle — one vertex per clock —
    // plus the pipeline's fill, which is what the +64 allows for.
    check(cycles <= 3 * static_cast<int>(batch.size()) + 64,
          "3 cycles per triangle in steady state", 3 * batch.size() + 64,
          static_cast<uint64_t>(cycles));
  }

  return zhao::report_and_exit("terrain_project_directed");
}
