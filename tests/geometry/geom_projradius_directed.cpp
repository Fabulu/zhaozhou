// geom_projradius_directed.cpp -- GEOM.LOD's `proj_radius_q8_i` against
// `zref::creature::projected_bound_radius_q8`, the law it IS.
// Owner ruling R68 sub-build 4.
//
// THE ORACLE IS CALLED, NOT TRANSCRIBED. Every expected value in this file
// comes from `zref::creature::projected_bound_radius_q8` itself, run on the
// same matrix the driver writes onto the projector configuration bus. Writing
// the formula into the test would be a second implementation of the law and
// would agree with a wrong RTL for exactly the reasons it was wrong.
//
// WHAT ACTUALLY DISCRIMINATES:
//
//   1. THE SNOOPER TAKES THE MAX OF ROW 0, ACROSS ALL THREE ELEMENTS. Four
//      cameras are written, each with the largest magnitude in a DIFFERENT
//      element of row 0 -- including one where m[0][0] is zero, which is a
//      camera looking along world X. An implementation that read m[0][0]
//      alone reports a zero projected radius for that camera, which the ladder
//      reads as "too small to see" and which nothing downstream can question.
//   2. THE TWO VIEWS DO NOT LEAK. View 1 is written a different matrix and a
//      different viewport, and both views are evaluated alternately.
//   3. A RANDOM SWEEP AGAINST THE ORACLE, 600 evaluations over four cameras,
//      radii from 1/256 m to 40 m and depths from 0.5 m to 900 m. This is
//      where the exact-reduction identity that lets 76 bits stand in for 82
//      is actually tested -- against the reference's __int128, not against the
//      identity's own algebra.
//   4. THE ROUNDING IS ROUND-HALF-UP, and it is checked at a CONSTRUCTED tie:
//      operands chosen so the remainder is exactly half the divisor, the one
//      input where truncation and qformats 3 differ. A uniform sweep hits it
//      essentially never.
//   5. BEHIND THE EYE IS NOT A ZERO RADIUS. The reference returns false and
//      `creature_sim` skips the creature; this block answers `ans_ok_o` low.
//      A caller that read a zero radius instead would demote a creature behind
//      the camera to the coarsest rung and the hysteresis would hold it there
//      for fifteen ticks after it came back.
//   6. THE TAG COMES BACK WITH ITS OWN ANSWER, under a stalled consumer and
//      across the refusal paths, which do not enter the divider at all.
//   7. EVERY COUNTER THAT CAN BE FIRED BY LEGAL STIMULUS IS FIRED. The one
//      that cannot -- `saturated_o` -- is stated as unreachable and left to a
//      committed mutant, not quietly asserted at zero.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vtb_geom_projradius.h"
#include "zhao_sim.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_fixp.hpp"

using zhao::check;

namespace {

constexpr int32_t kOne = 1 << 16;  // fx16 unity

// `zhao_project_core`'s configuration map, from its own port-list comment:
//   addr 0..15 : matrix row-major m[0..15]
//   addr 17    : { h[27:16], w[11:0] }
constexpr uint8_t kRectWhAddr = 17;

struct Camera {
  zref::mat4fx vp{};
  uint32_t viewport_w = 0;
};

/** A camera whose row 0 is authored directly, so the test can put the largest
 *  magnitude in whichever element it wants to. The remaining rows are a plain
 *  perspective shape: row 3 carries the w = z law the reference divides by. */
Camera make_camera(int32_t m00, int32_t m01, int32_t m02, int32_t m03, uint32_t vw) {
  Camera c;
  c.vp.m[0][0] = zref::fx16{m00};
  c.vp.m[0][1] = zref::fx16{m01};
  c.vp.m[0][2] = zref::fx16{m02};
  c.vp.m[0][3] = zref::fx16{m03};
  c.vp.m[1][1] = zref::fx16{static_cast<int32_t>(1.6 * kOne)};
  c.vp.m[2][2] = zref::fx16{kOne};
  // w = z: the depth the projector returns and the reference divides by.
  c.vp.m[3][2] = zref::fx16{kOne};
  c.viewport_w = vw;
  return c;
}

struct Bench {
  Vtb_geom_projradius& d;

  explicit Bench(Vtb_geom_projradius& dut) : d(dut) {}

  void idle() {
    d.cfg_we_i = 0;
    d.cfg_view_i = 0;
    d.cfg_addr_i = 0;
    d.cfg_data_i = 0;
    d.req_valid_i = 0;
    d.bound_radius_i = 0;
    d.w_i = 0;
    d.behind_i = 0;
    d.tag_i = 0;
    d.view_i = 0;
  }

  void cfg(int view, uint8_t addr, uint32_t data) {
    d.cfg_we_i = 1;
    d.cfg_view_i = view ? 1 : 0;
    d.cfg_addr_i = addr;
    d.cfg_data_i = data;
    d.eval();
    zhao::tick(d);
    idle();
    d.eval();
  }

  /** Write a whole camera: the sixteen matrix words and the viewport rect. */
  void write_camera(int view, const Camera& c) {
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
        cfg(view, static_cast<uint8_t>(4 * i + j),
            static_cast<uint32_t>(c.vp.m[i][j].raw));
    cfg(view, kRectWhAddr, c.viewport_w & 0xFFFu);
  }

  /** One evaluation. Returns `ok`; `out` gets the radius. `stall` holds the
   *  consumer's ready low for a while, so the answer must be HELD. */
  bool evaluate(int view, int32_t bound_radius, uint32_t w, bool behind, uint16_t tag,
                int32_t& out, int stall = 0) {
    d.view_i = view ? 1 : 0;
    d.req_valid_i = 1;
    d.bound_radius_i = bound_radius;
    d.w_i = w & 0x7FFFFFFFu;
    d.behind_i = behind ? 1 : 0;
    d.tag_i = tag;
    d.ans_ready_i = 0;
    d.eval();
    int guard = 0;
    while (!d.req_ready_o && guard++ < 1000) zhao::tick(d);
    zhao::tick(d);  // the accepting edge
    d.req_valid_i = 0;
    // The caller's operands go away immediately: the block must have latched
    // everything it needs on that edge.
    d.bound_radius_i = 0;
    d.w_i = 0;
    d.behind_i = 0;
    d.tag_i = 0;
    d.view_i = 0;
    d.eval();

    guard = 0;
    while (!d.ans_valid_o && guard++ < 4000) zhao::tick(d);
    check(guard < 4000, "the evaluation finished", 1, guard < 4000);
    for (int i = 0; i < stall; ++i) {
      zhao::tick(d);
      check(d.ans_valid_o != 0, "the answer is HELD while the consumer stalls", 1,
            d.ans_valid_o);
    }
    out = static_cast<int32_t>(d.radius_q8_o);
    const bool ok = d.ans_ok_o != 0;
    check(d.tag_o == tag, "the tag came back with its own answer", tag, d.tag_o);
    d.ans_ready_i = 1;
    d.eval();
    zhao::tick(d);
    d.ans_ready_i = 0;
    d.eval();
    return ok;
  }
};

/** The reference's own answer for the same inputs. `w` is returned too, so
 *  the driver feeds the RTL exactly the depth the projector would. */
bool oracle(const Camera& c, int32_t wx, int32_t wy, int32_t wz, int32_t bound_radius,
            int32_t& radius_q8, int32_t& w_out) {
  const zref::vec4fx clip = zref::mat4_vec4(
      c.vp,
      zref::vec4fx{zref::fx16{wx}, zref::fx16{wy}, zref::fx16{wz}, zref::fx16{kOne}},
      nullptr);
  w_out = clip.w.raw;
  return zref::creature::projected_bound_radius_q8(c.vp, wx, wy, wz, bound_radius,
                                                   c.viewport_w, radius_q8, nullptr);
}

struct Rng {
  uint64_t s = 0x9E3779B97F4A7C15ull;
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return static_cast<uint32_t>(s >> 32);
  }
  int32_t range(int32_t lo, int32_t hi) {
    return lo + static_cast<int32_t>(next() % static_cast<uint32_t>(hi - lo + 1));
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vtb_geom_projradius;  // heap + exit_hard: see zhao_sim.hpp
  Bench b(*top);
  b.idle();
  top->ans_ready_i = 0;
  top->rst_n = 0;
  top->eval();
  for (int i = 0; i < 3; ++i) zhao::tick(*top);
  top->rst_n = 1;
  top->eval();

  // ---- 1. four cameras, the max in a different element each time ----------
  // 1.75, 0.50, 0.25 -- the max is m[0][0], the ordinary case.
  const Camera camA = make_camera(static_cast<int32_t>(1.75 * kOne), kOne / 2, kOne / 4,
                                  0, 256);
  // 0.25, 2.00, 0.50 -- the max is m[0][1].
  const Camera camB = make_camera(kOne / 4, 2 * kOne, kOne / 2, 0, 320);
  // ZERO, 0.75, -3.00 -- the max is |m[0][2]| and m[0][0] is zero. This is a
  // camera looking along world X, and an implementation reading m[0][0] alone
  // reports a zero radius for every creature in it.
  const Camera camC = make_camera(0, (3 * kOne) / 4, -3 * kOne, 0, 256);
  // A negative maximum, so the MAGNITUDE is what wins rather than the value.
  const Camera camD = make_camera(-static_cast<int32_t>(2.5 * kOne), kOne, kOne / 8, 0, 192);

  b.write_camera(0, camA);
  b.write_camera(1, camB);
  check(top->kx0_o == static_cast<uint32_t>(1.75 * kOne),
        "kx0 is the largest row-0 magnitude", static_cast<uint32_t>(1.75 * kOne),
        top->kx0_o);
  check(top->vw0_o == 256, "and the viewport width came off address 17", 256, top->vw0_o);
  check(top->kx1_o == static_cast<uint32_t>(2 * kOne),
        "view 1's kx is ITS row-0 maximum, in a different element",
        static_cast<uint32_t>(2 * kOne), top->kx1_o);
  check(top->vw1_o == 320, "with its own viewport width", 320, top->vw1_o);

  // ---- 2. the oracle, on a hand-picked creature at a hand-picked depth ----
  {
    // A 1 m creature 10 m down the camera's z, in view 0.
    const int32_t bound = kOne;
    int32_t want = 0, w = 0;
    const bool ok = oracle(camA, 0, 0, 10 * kOne, bound, want, w);
    check(ok, "the reference answers for a creature in front of the eye", 1, ok);
    int32_t got = 0;
    const bool rtl_ok = b.evaluate(0, bound, static_cast<uint32_t>(w), false, 0x1234, got);
    check(rtl_ok, "and so does the RTL", 1, rtl_ok);
    check(got == want, "a 1 m creature at 10 m, view 0", static_cast<uint64_t>(want),
          static_cast<uint64_t>(got));
    // The number means something: 1.75 * 65536 * 256 * 128 / (10 * 65536 * 65536)
    // is about 87.5 raw = 0.34 screen pixels of half-extent.
    check(want > 0, "...and it is a positive number of subpixels", 1, want > 0);
  }

  // ---- 1b. the camera looking along world X, which is the discriminating one
  {
    b.write_camera(0, camC);
    check(top->kx0_o == static_cast<uint32_t>(3 * kOne),
          "a zero m[0][0] does NOT make kx zero -- |m[0][2]| wins",
          static_cast<uint32_t>(3 * kOne), top->kx0_o);
    const int32_t bound = 2 * kOne;
    int32_t want = 0, w = 0;
    const bool ok = oracle(camC, 0, 0, 25 * kOne, bound, want, w);
    check(ok, "the reference answers", 1, ok);
    int32_t got = 0;
    check(b.evaluate(0, bound, static_cast<uint32_t>(w), false, 0x00C0, got), "ok", 1, 1);
    check(got == want, "camera C: a 2 m creature at 25 m", static_cast<uint64_t>(want),
          static_cast<uint64_t>(got));
    check(want > 0,
          "and it is NOT zero, which is what m[0][0] alone would have produced", 1,
          want > 0);
  }

  // ---- 4. the rounding tie, CONSTRUCTED ----------------------------------
  // rhu(N/D) and floor(N/D) differ exactly when the remainder is EXACTLY D/2,
  // and such an input is not easy to stumble into here: with
  // rnum = kx * R * vw * 128 and rden = w << 16, a tie needs
  // val2(rnum) == 15 + val2(w) exactly -- one too few and the remainder is
  // the wrong size, one too many and it is zero. Worked out for camera A
  // (kx = 1.75 * 65536 = 7 * 2^14) with vw = 256 = 2^8 and the law's own
  // 128 = 2^7: val2(rnum) = 29 + val2(R), so an ODD bound radius against
  // w = 2^14 lands on it every time.
  //
  //   R = 3, w = 16384 (0.25 m -- a creature right at the camera)
  //   rnum = 3 * 7 * 2^29 = 11,274,289,152
  //   rden = 16384 << 16  =  1,073,741,824
  //   rnum / rden = 10.5 EXACTLY -> truncation 10, round-half-up 11
  //
  // A uniform sweep finds this with probability near 2^-40 per case.
  {
    b.write_camera(0, camA);   // kx = 114688, vw = 256
    const int32_t bound = 3;
    const uint32_t w = 16384;
    const uint64_t rnum = 114688ull * 3ull * 256ull * 128ull;
    const uint64_t rden = static_cast<uint64_t>(w) << 16;
    const int32_t want_rhu = static_cast<int32_t>((rnum + rden / 2) / rden);
    const int32_t want_trunc = static_cast<int32_t>(rnum / rden);
    check(rnum % rden == rden / 2,
          "the constructed input is an EXACT tie: the remainder is half the divisor", 1,
          rnum % rden == rden / 2);
    check(want_rhu == want_trunc + 1,
          "so round-half-up and truncation differ by one here", 1,
          want_rhu == want_trunc + 1);
    int32_t got = 0;
    check(b.evaluate(0, bound, w, false, 0x7E7E, got), "ok", 1, 1);
    check(got == want_rhu, "and the RTL rounds HALF-UP (qformats 3)",
          static_cast<uint64_t>(want_rhu), static_cast<uint64_t>(got));
    check(got != want_trunc, "...not down", 1, got != want_trunc);
  }

  // ---- 5. behind the eye is NOT a zero radius ----------------------------
  {
    int32_t got = 0;
    const uint32_t behind_before = top->behind_o;
    const bool ok = b.evaluate(0, kOne, 0, true, 0xBEEF, got);
    check(!ok, "behind the eye answers ans_ok LOW", 0, ok);
    check(got == 0, "with a zero radius that the caller must NOT read as a radius", 0,
          static_cast<uint64_t>(got));
    check(top->behind_o == behind_before + 1, "and it is counted", behind_before + 1,
          top->behind_o);
    // w == 0 is the same refusal with the other spelling.
    const bool ok2 = b.evaluate(0, kOne, 0, false, 0xBEE0, got);
    check(!ok2, "w == 0 is the same refusal (the reference's clip.w <= 0)", 0, ok2);
    check(top->behind_o == behind_before + 2, "counted the same way", behind_before + 2,
          top->behind_o);
  }

  // ---- 5b. a non-positive bound radius is refused, not divided by --------
  {
    int32_t got = 0;
    const uint32_t bad_before = top->bad_bound_o;
    check(!b.evaluate(0, 0, 4 * kOne, false, 0x0B0B, got),
          "a zero bound radius -- a ladder-bank MISS -- is refused", 0, 1);
    check(top->bad_bound_o == bad_before + 1, "and counted", bad_before + 1,
          top->bad_bound_o);
    check(!b.evaluate(0, -5, 4 * kOne, false, 0x0B0C, got),
          "so is a negative one", 0, 1);
    check(top->bad_bound_o == bad_before + 2, "and counted", bad_before + 2,
          top->bad_bound_o);
  }

  // ---- 6. the answer is HELD under a stalled consumer --------------------
  {
    b.write_camera(0, camA);
    const int32_t bound = 3 * kOne;
    int32_t want = 0, w = 0;
    check(oracle(camA, 0, 0, 7 * kOne, bound, want, w), "ok", 1, 1);
    int32_t got = 0;
    check(b.evaluate(0, bound, static_cast<uint32_t>(w), false, 0xABCD, got, 40), "ok", 1,
          1);
    check(got == want, "the held answer is still the right one",
          static_cast<uint64_t>(want), static_cast<uint64_t>(got));
  }

  // ---- 3. the random sweep, against the oracle ---------------------------
  {
    const Camera cams[4] = {camA, camB, camC, camD};
    Rng rng;
    int evaluated = 0;
    for (int i = 0; i < 600; ++i) {
      const int ci = i & 3;
      const int view = i & 1;
      b.write_camera(view, cams[ci]);
      // 1/256 m to 40 m of bound radius; 0.5 m to 900 m of depth.
      const int32_t bound = rng.range(kOne / 256, 40 * kOne);
      const int32_t wz = rng.range(kOne / 2, 900 * kOne);
      const int32_t wx = rng.range(-50 * kOne, 50 * kOne);
      const int32_t wy = rng.range(-20 * kOne, 20 * kOne);
      int32_t want = 0, w = 0;
      if (!oracle(cams[ci], wx, wy, wz, bound, want, w)) continue;
      if (w <= 0) continue;
      int32_t got = 0;
      const bool ok = b.evaluate(view, bound, static_cast<uint32_t>(w), false,
                                 static_cast<uint16_t>(i), got);
      char nm[96];
      std::snprintf(nm, sizeof nm, "sweep %d (camera %d, view %d)", i, ci, view);
      check(ok, nm, 1, ok);
      check(got == want, nm, static_cast<uint64_t>(want), static_cast<uint64_t>(got));
      ++evaluated;
    }
    check(evaluated > 400, "the sweep actually evaluated most of its cases", 1,
          evaluated > 400);
    std::printf("[geom_projradius_directed] sweep evaluated %d cases\n", evaluated);
  }

  // ---- 7b. |INT32_MIN| SATURATES, and is counted -------------------------
  // `std::abs` on INT32_MIN is undefined in C++ and wrapping is wrong in
  // hardware, so the snooper's magnitude saturates at INT32_MAX. A matrix
  // element at exactly -2^31 is -32768.0 in fx16 and is not a camera anyone
  // authored -- which is precisely why the counter is fired here rather than
  // asserted at zero.
  {
    const uint32_t before = top->abs_saturated_o;
    b.cfg(0, 0, 0x80000000u);
    check(top->abs_saturated_o == before + 1, "|INT32_MIN| saturates and is counted",
          before + 1, top->abs_saturated_o);
    check(top->kx0_o == 0x7FFFFFFFu, "...at INT32_MAX rather than wrapping to zero",
          0x7FFFFFFFu, top->kx0_o);
    // A write to an address this block does not own moves nothing.
    const uint32_t after = top->abs_saturated_o;
    b.cfg(0, 8, 0x80000000u);   // m[2][0] -- not row 0
    check(top->abs_saturated_o == after,
          "and a -2^31 written to an address it does not own is not its business", after,
          top->abs_saturated_o);
    check(top->kx0_o == 0x7FFFFFFFu, "...nor does it change kx", 0x7FFFFFFFu, top->kx0_o);
  }

  // ---- 7. the counters -----------------------------------------------------
  check(top->evaluations_o > 0 && top->behind_o > 0 && top->bad_bound_o > 0
            && top->abs_saturated_o > 0,
        "every counter reachable by legal stimulus was FIRED", 1,
        top->evaluations_o > 0 && top->behind_o > 0 && top->bad_bound_o > 0
            && top->abs_saturated_o > 0);
  // `saturated_o` is NOT asserted at zero and called evidence. It needs a
  // quotient past 2^31 subpixels -- 8.4 million -- which is 32,768 screen
  // pixels of half-extent and is unreachable with any legal viewport. It is
  // stated here as unreachable and owed a committed mutant, which is this
  // repo's rule for a guard no legal input can move.
  std::printf(
      "[geom_projradius_directed] evaluations=%u behind=%u bad_bound=%u saturated=%u "
      "(unreachable by legal stimulus) abs_saturated=%u\n",
      top->evaluations_o, top->behind_o, top->bad_bound_o, top->saturated_o,
      top->abs_saturated_o);
  top->final();
  return zhao::report_and_exit("geom_projradius_directed");
}
