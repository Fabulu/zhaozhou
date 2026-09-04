// skin_norm_rtl_directed.cpp — SKIN.NORM against zref::creature::skin_world_normal.
//
// The block owns one law and delegates the square root, so the comparison is
// the pair it emits: the blended direction and its magnitude, EXACTLY. Not the
// Lambert -- that is GEOM.LIGHT's, and the equivalence between "one normal, N
// lights" and "N independent calls" is proved separately in
// skin_norm_split_directed.cpp.
//
// The testbench PLAYS zhao_field_isqrt rather than instantiating it. That block
// has its own differential against `zref::isqrt_u64`, and instantiating it here
// would re-prove the square root while hiding whether THIS block handed it the
// right sum of squares.
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_geom_skin_norm.h"

#include "zhao_sim.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_trig.hpp"

namespace zc = zref::creature;

namespace {

constexpr int32_t ONE = 65536;

struct Rng {
  uint32_t s;
  explicit Rng(uint32_t x) : s(x) {}
  uint32_t next() {
    s = s * 1664525u + 1013904223u;
    return s;
  }
  int32_t sym(int32_t r) { return static_cast<int32_t>(next() % (2u * r + 1u)) - r; }
};

struct Observed {
  bool got = false;
  int64_t n[3] = {0, 0, 0};
  uint64_t mag = 0;
  bool degenerate = false;
};

// Drive one vertex, answering the isqrt service from the same zref::isqrt_u64
// the oracle uses -- so the two share one square root rather than two that
// agree until they do not.
Observed run(Vzhao_geom_skin_norm& t, const zc::mat3x4fx& A, const zc::mat3x4fx& B,
             const zc::SkinVertex& v) {
  Observed o;

  t.v_valid_i = 1;
  t.v_nx_i = v.nx;
  t.v_ny_i = v.ny;
  t.v_nz_i = v.nz;
  t.v_w0_i = v.w0;
  t.v_src_id_i = 0x33;
  for (int i = 0; i < 12; ++i) {
    t.a_i[i] = static_cast<uint32_t>(A.m[i]);
    t.b_i[i] = static_cast<uint32_t>(B.m[i]);
  }
  t.sq_ready_i = 0;
  t.sq_rvalid_i = 0;
  t.n_ready_i = 1;
  t.eval();
  zhao::tick(t);
  t.v_valid_i = 0;

  bool asked = false;
  uint64_t answer = 0;
  int delay = 0;

  for (int c = 0; c < 400; ++c) {
    t.sq_ready_i = 1;
    t.sq_rvalid_i = 0;
    if (asked && delay > 0 && --delay == 0) {
      t.sq_rvalid_i = 1;
      t.sq_r_i = answer;
    }
    t.eval();

    if (t.sq_valid_o && !asked) {
      asked = true;
      answer = zref::isqrt_u64(t.sq_n_o);
      delay = 2;
    }
    if (t.n_valid_o) {
      o.got = true;
      o.n[0] = static_cast<int64_t>(t.n_x_o);
      o.n[1] = static_cast<int64_t>(t.n_y_o);
      o.n[2] = static_cast<int64_t>(t.n_z_o);
      o.mag = t.n_mag_o;
      o.degenerate = t.n_degenerate_o != 0;
      zhao::tick(t);
      break;
    }
    zhao::tick(t);
  }
  t.sq_rvalid_i = 0;
  return o;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_geom_skin_norm top;

  top.v_valid_i = 0;
  top.sq_ready_i = 0;
  top.sq_rvalid_i = 0;
  top.n_ready_i = 1;
  top.rst_n = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  Rng r(0x5C1DE5u);

  // ---- 1: the pair, against the oracle, exactly ---------------------------
  {
    int bad = 0, compared = 0, live_cases = 0;
    for (int i = 0; i < 400; ++i) {
      zc::mat3x4fx A{}, B{};
      for (int k = 0; k < 12; ++k) A.m[k] = r.sym(2 * ONE);
      for (int k = 0; k < 12; ++k) B.m[k] = r.sym(2 * ONE);

      zc::SkinVertex v{};
      v.nx = static_cast<int8_t>(r.sym(127));
      v.ny = static_cast<int8_t>(r.sym(127));
      v.nz = static_cast<int8_t>(r.sym(127));
      v.b0 = 0;
      v.b1 = 1;
      v.w0 = static_cast<uint8_t>(r.next() % 65u);

      const zc::mat3x4fx pal[2] = {A, B};
      int64_t wn[3];
      int64_t wmag = 0;
      const bool live = zc::skin_world_normal(pal, v, wn, &wmag);

      const Observed o = run(top, A, B, v);
      ++compared;
      if (live) ++live_cases;

      const bool ok = o.got && (o.degenerate == !live) &&
                      (!live || (o.n[0] == wn[0] && o.n[1] == wn[1] && o.n[2] == wn[2] &&
                                 o.mag == static_cast<uint64_t>(wmag)));
      if (!ok) {
        if (bad < 3)
          std::printf(
              "    rtl (%lld,%lld,%lld) mag %llu deg %d | oracle (%lld,%lld,%lld) mag %lld live "
              "%d\n",
              (long long)o.n[0], (long long)o.n[1], (long long)o.n[2], (unsigned long long)o.mag,
              o.degenerate ? 1 : 0, (long long)wn[0], (long long)wn[1], (long long)wn[2],
              (long long)wmag, live ? 1 : 0);
        ++bad;
      }
    }
    zhao::check(bad == 0,
                "the blended direction and its magnitude match "
                "zref::creature::skin_world_normal EXACTLY -- the RTL accumulates "
                "in 64 bits because the oracle does, so a narrowing on either "
                "side would part company only on large coordinates",
                0, bad);
    zhao::check(compared == 400, "every vertex retired", 400, compared);
    zhao::check(live_cases > 300,
                "and the great majority are LIVE -- a sweep that agreed only "
                "about degenerate vertices would prove nothing",
                1, (live_cases > 300) ? 1 : 0);
  }

  // ---- 2: a zero packed normal is degenerate, before the palette is touched
  {
    zc::mat3x4fx A{}, B{};
    for (int k = 0; k < 12; ++k) A.m[k] = r.sym(2 * ONE);
    for (int k = 0; k < 12; ++k) B.m[k] = r.sym(2 * ONE);
    zc::SkinVertex v{};
    v.nx = 0;
    v.ny = 0;
    v.nz = 0;
    v.b0 = 0;
    v.b1 = 1;
    v.w0 = 32;

    const uint32_t before = top.degenerate_o;
    const Observed o = run(top, A, B, v);
    zhao::check(o.got && o.degenerate && o.mag == 0 && top.degenerate_o == before + 1,
                "a zero packed normal is reported degenerate and counted -- the "
                "surface has no direction and GEOM.LIGHT must leave it black "
                "rather than receive a zero-length vector to divide by",
                1, 1);
  }

  // ---- 3: identity bones, +X normal -- a value that can be read by eye ----
  {
    zc::mat3x4fx I{};
    I.m[0] = ONE;
    I.m[5] = ONE;
    I.m[10] = ONE;
    zc::SkinVertex v{};
    v.nx = 127;
    v.ny = 0;
    v.nz = 0;
    v.b0 = 0;
    v.b1 = 1;
    v.w0 = 32;

    const zc::mat3x4fx pal[2] = {I, I};
    int64_t wn[3];
    int64_t wmag = 0;
    zc::skin_world_normal(pal, v, wn, &wmag);

    const Observed o = run(top, I, I, v);
    zhao::check(o.got && !o.degenerate && o.n[1] == 0 && o.n[2] == 0 && o.n[0] == wn[0] &&
                    o.mag == static_cast<uint64_t>(wmag),
                "identity bones and a +X normal give a pure +X direction, and "
                "the magnitude is the oracle's -- a case whose answer can be "
                "read without running anything",
                1, 1);
  }

  std::printf("  %u vertices, %u degenerate, %u range-reduced\n", top.vertices_o, top.degenerate_o,
              top.reduced_o);
  return zhao::report_and_exit("skin_norm_rtl_directed");
}
