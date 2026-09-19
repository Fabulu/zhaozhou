// light_skin_adapter_directed.cpp -- the creature seam, tested as a REFUSAL
// and not as a cast.
//
// WHAT IS ACTUALLY AT STAKE HERE
// ---------------------------------------------------------------------------
// `skin_world_normal` hands out a range-reduced direction, s64x3. The lighting
// service consumes s32x3 (and, since owner ruling R31, computes the magnitude
// itself with its own II8 root -- none crosses this seam). Narrowing is a
// CLAIM about the producer's range reduction, and the failure mode if the
// claim is wrong is the worst kind: a truncated lane is still a perfectly
// plausible normal, so the vertex comes out lit, in range, and computed from a
// different vector. No output check can see it. Only a refusal at the port can.
//
// So this bench does two things:
//   * DIFFERENTIAL, on tuples produced by the COMPILED producer itself. It
//     calls `zref::creature::skin_world_normal` -- not a model of it -- and
//     asserts every tuple the real producer emits is ACCEPTED and passes
//     through bit-exact. If the range-reduction claim were wrong, the real
//     producer would generate the counterexample.
//   * REFUSAL, on tuples outside that domain. Out-of-range values are legal
//     inputs to the public C++ entry point and must be REFUSED at this hot
//     port, counted, and never narrowed -- the vertex still crosses, IN ORDER,
//     as a zero, declared-degenerate record (2026-09-19, so GEOM.VATTR's colour
//     ordinals cannot shift), but its direction never does.
//
// THE BOUND IS THE ONE THAT IS TRUE, NOT THE ONE THAT LOOKS TIDY. The
// producer's loop is `while (max|n| >= 2^30) n >>= 1`, and an arithmetic right
// shift of a negative odd value rounds toward minus infinity, so a lane can
// land on exactly -2^30 while the tracked maximum is below it. Asserting
// |n| < 2^30 would reject a legal tuple. The adapter asserts the property that
// actually holds -- the value sign-extends from 32 bits -- and section 3 feeds
// it -2^30 explicitly.
//
// POSITIVE CONTROL: --break-oracle corrupts one expected lane by +1.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_light_skin_adapter.h"

#include "zhao_sim.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_trig.hpp"

using zhao::check;

namespace {

void tk(Vzhao_light_skin_adapter& d) { zhao::tick(d); }

void reset_dut(Vzhao_light_skin_adapter& d) {
  d.rst_n = 0;
  d.s_valid_i = 0;
  d.p_ready_i = 0;
  d.s_nx_i = 0;
  d.s_ny_i = 0;
  d.s_nz_i = 0;
  d.s_degenerate_i = 0;
  d.s_nlights_i = 0;
  d.s_src_id_i = 0;
  d.eval();
  for (int i = 0; i < 3; ++i) tk(d);
  d.rst_n = 1;
  d.eval();
  tk(d);
}

struct Tuple {
  int64_t n[3];
  bool degen;
  uint8_t nlights;
  uint16_t src;
};

struct Res {
  bool accepted = false;
  int32_t n[3] = {0, 0, 0};
  bool degen = false;
  uint16_t src = 0;
};

// Offer one tuple and drain whatever comes out within a bounded window.
Res offer(Vzhao_light_skin_adapter& d, const Tuple& t) {
  Res r;
  d.p_ready_i = 1;
  d.s_valid_i = 1;
  d.s_nx_i = static_cast<uint64_t>(t.n[0]);
  d.s_ny_i = static_cast<uint64_t>(t.n[1]);
  d.s_nz_i = static_cast<uint64_t>(t.n[2]);
  d.s_degenerate_i = t.degen ? 1 : 0;
  d.s_nlights_i = t.nlights;
  d.s_src_id_i = t.src;
  d.eval();
  int guard = 0;
  while (!d.s_ready_o) {
    tk(d);
    d.eval();
    if (++guard > 100) break;
  }
  tk(d);
  d.s_valid_i = 0;
  d.eval();
  for (int i = 0; i < 4 && !d.p_valid_o; ++i) {
    tk(d);
    d.eval();
  }
  if (d.p_valid_o) {
    r.accepted = true;
    r.n[0] = static_cast<int32_t>(d.p_nx_o);
    r.n[1] = static_cast<int32_t>(d.p_ny_o);
    r.n[2] = static_cast<int32_t>(d.p_nz_o);
    r.degen = d.p_degenerate_o != 0;
    r.src = static_cast<uint16_t>(d.p_src_id_o);
    tk(d);
  }
  d.p_ready_i = 0;
  d.eval();
  return r;
}

uint64_t g_rng = 0x6C8E9CF570932BD5ULL;
uint64_t rnd() {
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 7;
  g_rng ^= g_rng << 17;
  return g_rng;
}

}  // namespace

int main(int argc, char** argv) {
  bool break_oracle = false;
  for (int i = 1; i < argc; ++i)
    if (!std::strcmp(argv[i], "--break-oracle")) break_oracle = true;

  Verilated::commandArgs(argc, argv);
  Vzhao_light_skin_adapter* dutp = new Vzhao_light_skin_adapter;  // heap, harness rule
  Vzhao_light_skin_adapter& dut = *dutp;
  reset_dut(dut);

  // ---- 1: THE ORDINARY CASE -----------------------------------------------
  {
    Tuple t{{3000, -40000, 9000}, false, 4, 0x0101};
    const Res r = offer(dut, t);
    check(r.accepted, "an in-domain tuple is accepted", 1, r.accepted ? 1 : 0);
    check(r.n[0] == 3000 && r.n[1] == -40000 && r.n[2] == 9000, "the direction crosses unchanged",
          3000, static_cast<uint32_t>(r.n[0]));
    check(r.src == 0x0101, "the source id rides the tuple", 0x0101, r.src);
    check(dut.accepted_o == 1, "accepted_o moved by exactly 1", 1, dut.accepted_o);
    check(dut.refused_o == 0, "refused_o did not move on a legal tuple", 0, dut.refused_o);
  }

  // ---- 2: THE COMPILED PRODUCER'S OWN TUPLES ------------------------------
  // Not a model of the producer: the producer. If the range-reduction claim in
  // the adapter's header were wrong, this is where the real counterexample
  // would arrive, and it would arrive as a REFUSAL rather than as a wrong
  // colour three blocks downstream.
  int produced = 0, degenerate = 0;
  {
    const uint32_t a0 = dut.accepted_o;
    const uint32_t r0 = dut.refused_o;
    zref::creature::mat3x4fx palette[4];
    for (int p = 0; p < 4; ++p)
      for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 4; ++col)
          palette[p].m[row * 4 + col] = static_cast<int32_t>(rnd() % 400001) - 200000;

    for (int i = 0; i < 500; ++i) {
      zref::creature::SkinVertex v{};
      v.nx = static_cast<int8_t>(rnd());
      v.ny = static_cast<int8_t>(rnd());
      v.nz = static_cast<int8_t>(rnd());
      v.b0 = static_cast<uint8_t>(rnd() & 3);
      v.b1 = static_cast<uint8_t>(rnd() & 3);
      v.w0 = static_cast<uint8_t>(rnd() & 0xFF);
      int64_t n[3];
      int64_t mag = 0;  // the stream's root recomputes it; not carried here
      const bool ok = zref::creature::skin_world_normal(palette, v, n, &mag);
      if (!ok) {
        ++degenerate;
        continue;
      }
      ++produced;
      Tuple t{{n[0], n[1], n[2]}, false, 4, static_cast<uint16_t>(i)};
      const Res r = offer(dut, t);
      check(r.accepted, "EVERY tuple the compiled producer emits is IN DOMAIN and accepted", 1,
            r.accepted ? 1 : 0);
      const int32_t inject = (break_oracle && i == 4) ? 1 : 0;
      check(r.n[0] == static_cast<int32_t>(n[0]) + inject, "lane x is lossless",
            static_cast<uint32_t>(n[0]) + inject, static_cast<uint32_t>(r.n[0]));
      check(r.n[1] == static_cast<int32_t>(n[1]), "lane y is lossless", static_cast<uint32_t>(n[1]),
            static_cast<uint32_t>(r.n[1]));
      check(r.n[2] == static_cast<int32_t>(n[2]), "lane z is lossless", static_cast<uint32_t>(n[2]),
            static_cast<uint32_t>(r.n[2]));
    }
    check(produced > 300, "coverage: the producer actually produced tuples", 300,
          static_cast<uint32_t>(produced));
    check(dut.accepted_o == a0 + static_cast<uint32_t>(produced),
          "accepted_o counted exactly the producer's tuples", a0 + static_cast<uint32_t>(produced),
          dut.accepted_o);
    check(dut.refused_o == r0, "not one tuple from the real producer was refused", r0,
          dut.refused_o);
  }

  // ---- 3: THE SHIFT BOUNDARY, WHICH IS -2^30 AND NOT -(2^30 - 1) ----------
  {
    const uint32_t a0 = dut.accepted_o;
    Tuple t{{-(int64_t{1} << 30), (int64_t{1} << 30) - 1, 0}, false, 4, 0x0301};
    const Res r = offer(dut, t);
    check(r.accepted, "the exact -2^30 boundary the producer's shift can reach is ACCEPTED", 1,
          r.accepted ? 1 : 0);
    check(r.n[0] == -(int32_t{1} << 30), "and it crosses unchanged",
          static_cast<uint32_t>(-(int32_t{1} << 30)), static_cast<uint32_t>(r.n[0]));
    check(dut.accepted_o == a0 + 1, "accepted_o moved by exactly 1", a0 + 1, dut.accepted_o);
    // INT32_MIN is the widest value that still sign-extends from 32 bits, so
    // it is in domain too. The adapter's test is representability, not a
    // guessed magnitude bound.
    Tuple t2{{INT32_MIN, INT32_MAX, 0}, false, 4, 0x0302};
    const Res r2 = offer(dut, t2);
    check(r2.accepted, "INT32_MIN still sign-extends from 32 bits and is accepted", 1,
          r2.accepted ? 1 : 0);
  }

  // ---- 4: REFUSAL, NOT TRUNCATION -----------------------------------------
  // Each of these is a value the public C++ entry point would accept and this
  // HOT PORT must not narrow. The counter is not the evidence on its own: the
  // evidence is WHAT came out -- one record, zero lanes, declared degenerate,
  // so the vertex keeps its place in the stream and its direction is lost
  // rather than faked.
  {
    const uint32_t a0 = dut.accepted_o;
    const uint32_t r0 = dut.refused_o;
    const std::vector<Tuple> bad = {
        {{int64_t{1} << 31, 0, 0}, false, 4, 0x0401},         // x just past s32
        {{0, -(int64_t{1} << 31) - 1, 0}, false, 4, 0x0402},  // y just past s32
        {{0, 0, int64_t{1} << 40}, false, 4, 0x0403},         // z far past s32
    };
    for (size_t i = 0; i < bad.size(); ++i) {
      const Res r = offer(dut, bad[i]);
      check(r.accepted && r.degen && r.n[0] == 0 && r.n[1] == 0 && r.n[2] == 0,
            "an out-of-domain tuple is NOT narrowed: exactly one record crosses, zero "
            "lanes, declared degenerate -- one output per input, its direction dropped",
            1, (r.accepted && r.degen && r.n[0] == 0 && r.n[1] == 0 && r.n[2] == 0) ? 1 : 0);
      check(r.src == bad[i].src, "and it keeps its source id, so the vertex is still named",
            bad[i].src, r.src);
    }
    check(dut.refused_o == r0 + bad.size(), "refused_o moved by exactly the refusal count",
          r0 + static_cast<uint32_t>(bad.size()), dut.refused_o);
    check(dut.accepted_o == a0, "accepted_o did NOT move: no DIRECTION crossed the seam", a0,
          dut.accepted_o);

    // And the adapter is still alive afterwards: a refusal must not wedge it.
    Tuple good{{7, 8, 9}, false, 4, 0x0406};
    const Res r = offer(dut, good);
    check(r.accepted, "a legal tuple after three refusals still crosses", 1, r.accepted ? 1 : 0);
    check(r.n[0] == 7 && r.n[1] == 8 && r.n[2] == 9, "and is unchanged", 7,
          static_cast<uint32_t>(r.n[0]));
  }

  // ---- 5: DEGENERACY IS FORWARDED, NOT JUDGED -----------------------------
  // The adapter has no opinion about whether a zero-length blend agrees with
  // the producer's flag. Forwarding both, unmodified, is what keeps the two
  // sides of the downstream seam detector independent.
  {
    Tuple t{{0, 0, 0}, true, 4, 0x0501};
    const Res r = offer(dut, t);
    check(r.accepted, "a declared-degenerate tuple still crosses", 1, r.accepted ? 1 : 0);
    check(r.degen, "the producer's degenerate flag is FORWARDED, not re-derived", 1,
          r.degen ? 1 : 0);

    Tuple t2{{1, 0, 0}, true, 4, 0x0502};
    const Res r2 = offer(dut, t2);
    check(r2.degen,
          "a flag that DISAGREES with the direction is forwarded too -- judging it here "
          "would collapse the downstream seam detector's two independent operands",
          1, r2.degen ? 1 : 0);
  }

  std::printf(
      "[skin] producer tuples: %d accepted, %d degenerate-rejected upstream | "
      "adapter accepted=%u refused=%u\n",
      produced, degenerate, dut.accepted_o, dut.refused_o);

  dut.final();
  zhao::exit_hard(zhao::report_and_exit("light_skin_adapter_directed"));
}
