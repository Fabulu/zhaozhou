// skin_norm_rtl_directed.cpp — SKIN.NORM against zref::creature::skin_world_normal.
//
// The block owns the blend and the range reduction; since owner ruling R31
// (2026-09-19) the magnitude is GEOM.LIGHT's II8 root, computed from THIS
// block's output. So the comparison is the direction, EXACTLY, and the root
// identity the downstream root depends on: isqrt_u64 of the emitted
// direction's sum of squares must equal the oracle's magnitude on every live
// vertex -- that is the whole claim that moving the root moved no law. Not the
// Lambert -- that is GEOM.LIGHT's, and "one normal, N lights" against "N
// independent calls" is proved separately in skin_norm_split_directed.cpp.
//
// Section 4 MEASURES the rate: vertices offered back to back, clocks counted.
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
  uint64_t mag = 0;  // isqrt_u64 of the emitted direction: what GEOM.LIGHT will root
  bool degenerate = false;
};

uint64_t root_of(const int64_t n[3]) {
  const uint64_t s = uint64_t(n[0] * n[0]) + uint64_t(n[1] * n[1]) + uint64_t(n[2] * n[2]);
  return zref::isqrt_u64(s);
}

// Drive one vertex and collect the direction it emits.
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
  t.n_ready_i = 1;
  t.eval();
  zhao::tick(t);
  t.v_valid_i = 0;

  for (int c = 0; c < 400; ++c) {
    t.eval();
    if (t.n_valid_o) {
      o.got = true;
      o.n[0] = static_cast<int64_t>(t.n_x_o);
      o.n[1] = static_cast<int64_t>(t.n_y_o);
      o.n[2] = static_cast<int64_t>(t.n_z_o);
      o.mag = o.n[0] == 0 && o.n[1] == 0 && o.n[2] == 0 ? 0 : root_of(o.n);
      o.degenerate = t.n_degenerate_o != 0;
      zhao::tick(t);
      break;
    }
    zhao::tick(t);
  }
  return o;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_geom_skin_norm top;

  top.v_valid_i = 0;
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
                "the blended direction matches zref::creature::skin_world_normal "
                "EXACTLY, and isqrt_u64 of its sum of squares -- the root "
                "GEOM.LIGHT now takes -- equals the oracle's magnitude. The RTL "
                "accumulates in 64 bits because the oracle does, so a narrowing "
                "on either side would part company only on large coordinates",
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

  // ---- 4: THE RATE (owner ruling R31) -------------------------------------
  // Vertices offered back to back with the consumer always ready. The block is
  // one-at-a-time, so the figure is its whole cost per vertex: accept, three
  // lane products, the reduction (plus one clock per range-reduction shift),
  // emit. Before R31 the same loop also waited out a 32-step serial root.
  {
    zc::mat3x4fx A{}, B{};
    for (int k = 0; k < 12; ++k) A.m[k] = r.sym(ONE);
    for (int k = 0; k < 12; ++k) B.m[k] = r.sym(ONE);
    for (int i = 0; i < 12; ++i) {
      top.a_i[i] = static_cast<uint32_t>(A.m[i]);
      top.b_i[i] = static_cast<uint32_t>(B.m[i]);
    }
    const uint32_t v0 = top.vertices_o;
    const uint32_t red0 = top.reduced_o;
    constexpr int kN = 200;
    int emitted = 0;
    long clocks = 0;
    top.n_ready_i = 1;
    while (emitted < kN && clocks < 100000) {
      top.v_valid_i = (static_cast<int>(top.vertices_o - v0) < kN) ? 1 : 0;
      top.v_nx_i = static_cast<int8_t>(r.sym(127));
      top.v_ny_i = static_cast<int8_t>(r.sym(127));
      top.v_nz_i = static_cast<int8_t>(r.sym(127));
      top.v_w0_i = static_cast<uint8_t>(r.next() % 65u);
      top.eval();
      if (top.n_valid_o && top.n_ready_i) ++emitted;
      zhao::tick(top);
      ++clocks;
    }
    top.v_valid_i = 0;
    const double per = static_cast<double>(clocks) / kN;
    std::printf("  RATE: %d vertices in %ld clocks = %.2f clocks/vertex (%u range-reduction shifts)\n",
                kN, clocks, per, top.reduced_o - red0);
    zhao::check(emitted == kN, "every rate vertex retired", kN, emitted);
    // GEOM.SKIN retires one vertex per twelve clocks (its header); the fork
    // stalls the skinner only if this block is slower than that.
    zhao::check(per <= 12.0,
                "SKIN.NORM costs no more than GEOM.SKIN's twelve clocks a vertex, so "
                "the AND-fork no longer stalls the skinner",
                12, static_cast<uint32_t>(per));
  }

  std::printf("  %u vertices, %u degenerate, %u range-reduced\n", top.vertices_o, top.degenerate_o,
              top.reduced_o);
  return zhao::report_and_exit("skin_norm_rtl_directed");
}
