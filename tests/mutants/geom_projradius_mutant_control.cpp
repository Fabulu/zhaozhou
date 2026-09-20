// geom_projradius_mutant_control.cpp -- the POSITIVE CONTROL for
// `zhao_geom_projradius`'s `saturated_o`.
//
// INVERTED POLARITY: this passes when the counter FIRES and when the answer is
// clamped. It is evidence about the INSTRUMENT, not about the design.
//
// WHY A MUTANT AND NOT STIMULUS. `saturated_o` watches for a quotient past
// 2^31 subpixels -- 8,388,608 screen pixels of half-extent. The quotient is
// kx * bound_radius * viewport_w / (2 * w), and with `viewport_w` bounded by
// the twelve bits the projector's configuration address 17 carries, reaching
// 2^31 needs either a creature larger than the world or a depth under one
// 65,536th of a metre. No legal camera produces it, so "the counter can fire"
// would stay an argument forever. `tests/mutants/zhao_geom_projradius_mutant.sv`
// narrows the overflow test from bit 31 to bit 12; everything else in that file
// is production, and `tools/budget/mutant_copy_drift.py` watches for it going
// stale.
//
// The control checks three things, because a counter that moves is not the same
// as a saturation path that works:
//   * the counter FIRES on a quotient the mutant calls too large;
//   * the answer is CLAMPED to INT32_MAX rather than wrapped or truncated;
//   * a quotient BELOW the mutant's narrowed bound still comes out exact, so
//     the copy is measuring the guard and not simply broken everywhere.
#include <cstdint>
#include <cstdio>

#include "Vzhao_geom_projradius_mutant.h"
#include "zhao_sim.hpp"

using zhao::check;

namespace {

constexpr int32_t kOne = 1 << 16;

struct Bench {
  Vzhao_geom_projradius_mutant& d;

  explicit Bench(Vzhao_geom_projradius_mutant& dut) : d(dut) {}

  bool evaluate(uint32_t kx, uint32_t vw, int32_t bound, uint32_t w, int32_t& out) {
    d.req_valid_i = 1;
    d.kx_i = kx;
    d.vw_i = vw;
    d.bound_radius_i = bound;
    d.w_i = w & 0x7FFFFFFFu;
    d.behind_i = 0;
    d.tag_i = 0;
    d.ans_ready_i = 0;
    d.eval();
    int guard = 0;
    while (!d.req_ready_o && guard++ < 1000) zhao::tick(d);
    zhao::tick(d);
    d.req_valid_i = 0;
    d.eval();
    guard = 0;
    while (!d.ans_valid_o && guard++ < 4000) zhao::tick(d);
    check(guard < 4000, "the mutant evaluation finished", 1, guard < 4000);
    out = static_cast<int32_t>(d.radius_q8_o);
    const bool ok = d.ans_ok_o != 0;
    d.ans_ready_i = 1;
    d.eval();
    zhao::tick(d);
    d.ans_ready_i = 0;
    d.eval();
    return ok;
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* dutp = new Vzhao_geom_projradius_mutant;
  Vzhao_geom_projradius_mutant& dut = *dutp;
  Bench b(dut);

  dut.req_valid_i = 0;
  dut.kx_i = 0;
  dut.vw_i = 0;
  dut.bound_radius_i = 0;
  dut.w_i = 0;
  dut.behind_i = 0;
  dut.tag_i = 0;
  dut.ans_ready_i = 0;
  dut.rst_n = 0;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  check(dut.saturated_o == 0, "the counter starts at zero", 0, dut.saturated_o);

  // ---- BELOW the narrowed bound: the copy still computes exactly ----------
  // kx = 1.75, R = 1 m, vw = 256, w = 100 m.
  {
    const uint32_t kx = static_cast<uint32_t>(1.75 * kOne);
    const uint32_t vw = 256;
    const int32_t bound = kOne;
    const uint32_t w = 100u * static_cast<uint32_t>(kOne);
    const uint64_t rnum = static_cast<uint64_t>(kx) * static_cast<uint64_t>(bound) * vw * 128ull;
    const uint64_t rden = static_cast<uint64_t>(w) << 16;
    const int32_t want = static_cast<int32_t>((rnum + rden / 2) / rden);
    check(want < 4096, "the case is BELOW the mutant's narrowed bound", 1, want < 4096);
    int32_t got = 0;
    check(b.evaluate(kx, vw, bound, w, got), "it answers", 1, 1);
    check(got == want, "and the broken copy is exact there -- it is the GUARD that moved",
          static_cast<uint64_t>(want), static_cast<uint64_t>(got));
    check(dut.saturated_o == 0, "with the counter still at zero", 0, dut.saturated_o);
  }

  // ---- ABOVE it: THE COUNTER MUST FIRE -----------------------------------
  // The same camera with the creature 0.5 m away instead of 100 m: the
  // quotient crosses 4,096, which production would carry exactly and this copy
  // must clamp.
  {
    const uint32_t kx = static_cast<uint32_t>(1.75 * kOne);
    const uint32_t vw = 256;
    const int32_t bound = kOne;
    const uint32_t w = static_cast<uint32_t>(kOne) / 2u;
    const uint64_t rnum = static_cast<uint64_t>(kx) * static_cast<uint64_t>(bound) * vw * 128ull;
    const uint64_t rden = static_cast<uint64_t>(w) << 16;
    const int32_t honest = static_cast<int32_t>((rnum + rden / 2) / rden);
    check(honest >= 4096, "the case is ABOVE the mutant's narrowed bound", 1,
          honest >= 4096);
    check(honest < 0x7FFFFFFF, "and PRODUCTION would answer it exactly", 1,
          honest < 0x7FFFFFFF);
    int32_t got = 0;
    check(b.evaluate(kx, vw, bound, w, got), "it answers", 1, 1);
    // THE INVERTED ASSERTIONS. This suite passes because these hold.
    check(dut.saturated_o == 1, "POSITIVE CONTROL: saturated_o FIRED", 1,
          dut.saturated_o);
    check(got == 0x7FFFFFFF,
          "POSITIVE CONTROL: the answer CLAMPED to INT32_MAX rather than wrapping",
          0x7FFFFFFFu, static_cast<uint64_t>(got));
    check(got != honest, "...and is therefore NOT the honest quotient", 1, got != honest);
  }

  // ---- and it counts each one, not just the first ------------------------
  {
    int32_t got = 0;
    b.evaluate(static_cast<uint32_t>(1.75 * kOne), 256, kOne,
               static_cast<uint32_t>(kOne) / 4u, got);
    check(dut.saturated_o == 2, "POSITIVE CONTROL: it counts every saturation", 2,
          dut.saturated_o);
  }

  std::printf("[geom_projradius_mutant_control] saturated=%u (inverted polarity: this "
              "suite passes because the counter FIRED)\n",
              dut.saturated_o);
  dut.final();
  return zhao::report_and_exit("geom_projradius_mutant_control");
}
