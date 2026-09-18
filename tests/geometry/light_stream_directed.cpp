// light_stream_directed.cpp -- the streamed lighting service, qualified
// against the COMPILED laws and measured on the ruled fixture.
//
// ---------------------------------------------------------------------------
// WHAT HAS AN ORACLE HERE
// ---------------------------------------------------------------------------
// TWO, and they are the two this refactor exists to reconcile:
//
//   render   : `zref::render::shade_from_world_normal_unclamped`
//   creature : `zref::creature::lambert_from_world_normal`
//
// Both are linked and called. Neither is restated in C++, and NEITHER is
// swapped for the other: section 10 drives the render profile against the
// render oracle and section 11 drives the creature profile against the
// creature oracle, on tuples shaped like the actual producer's output.
//
// The differential is readable off `rgb_r_o` without a private probe because
// with colour gain 0x10000, emission 0, detail 0 and zero environment,
// `(0x10000 * ndl + 32768) >> 16 == ndl` exactly. So the composed block's red
// channel IS the light response, and the comparison is against the shipped
// function rather than against this file's reading of it.
//
// THE COLOUR FOLD STILL HAS NO RATIFIED ORACLE. GEOM.LIGHT.md says so and
// nothing here pretends otherwise: sections 2..8 carry hand-computable
// constants (0x10000, 0x8000, 0x4000, and the fused-product pair 2 vs 1) that
// a reader checks with a pencil, because a fold model written in this file and
// compared against RTL written from the same prose is a duplicate checked
// against itself.
//
// ---------------------------------------------------------------------------
// THE THREE TRAPS THE HANDOFF NAMES, EACH WITH ITS OWN DIRECTED SECTION
// ---------------------------------------------------------------------------
//  5. GAIN AND EMISSION MUST NOT BE COMBINED BEFORE MULTIPLICATION. With both
//     coefficients 1 and the response 32768, separately rounded terms sum to
//     2 and the fused product rounds to 1. Section 5 constructs exactly that
//     response and asserts 2.
//  13/14. DO NOT CLONE THE SCALAR ENGINE TO MAKE THE RATE, and EIGHT LIGHTS
//     ON EVERY VERTEX IS OVER BUDGET. Section 14 runs the 960,000-term
//     workload and asserts it EXCEEDS the frame -- a test that would go green
//     if somebody quietly relabelled the workload. Section 15's product-slot
//     conservation law (every clock offers exactly two lane slots, and
//     dot + square + unused accounts for all of them) is what a cloned engine
//     would break, because a second engine's products are not in this budget.
//
// ---------------------------------------------------------------------------
// COUNTERS
// ---------------------------------------------------------------------------
// Every counter is read as a DELTA across its own case and again as a total,
// because a total that happens to match says nothing about which case moved
// it. `tag_mismatch_o` is the one detector whose two operands genuinely travel
// by different paths -- the divider's own tag through sixteen folded stages,
// the side entry through a FIFO beside it -- and section 15 asserts it stayed
// zero only AFTER section 16 has fired it deliberately on a mutant.
//
// POSITIVE CONTROL: --break-oracle corrupts ONE expected value by +1 in each
// differential tier; the suite must then FAIL.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_light_stream.h"

#include "zhao_sim.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_terrain_shade.hpp"
#include "zref/zref_trig.hpp"

using zhao::check;

namespace {

constexpr int32_t  kOne = 0x10000;
constexpr uint32_t kLightsMax = 8;
constexpr uint32_t kEnvIdx = 0xF;

constexpr int32_t KLX = zref::terrain::kShadeLightX;
constexpr int32_t KLY = zref::terrain::kShadeLightY;
constexpr int32_t KLZ = zref::terrain::kShadeLightZ;

struct Light {
  int32_t lx = 0, ly = 0, lz = 0;
  int32_t detail = 0;
  uint32_t cr = 0, cg = 0, cb = 0;
  uint32_t er = 0, eg = 0, eb = 0;
};

struct Env {
  uint32_t ar = 0, ag = 0, ab = 0;
  uint32_t sr = 0, sg = 0, sb = 0;
};

struct Vtx {
  int32_t nx = 0, ny = 0, nz = 0;
  bool     mag_valid = false;
  uint32_t mag = 0;
  bool     producer_degen = false;
  bool     creature = false;   // profile: 1 = creature (no detail, early clamp)
  uint32_t nlights = 1;
  uint16_t src = 0;
};

struct Rgb { uint32_t r = 0, g = 0, b = 0; };

struct Out {
  Rgb rgb;
  bool degen = false;
  uint16_t src = 0;
};

uint64_t g_cycles = 0;
void tk(Vzhao_light_stream& d) {
  zhao::tick(d);
  ++g_cycles;
}

void reset_dut(Vzhao_light_stream& d) {
  d.rst_n = 0;
  d.cfg_we_i = 0;
  d.cfg_commit_i = 0;
  d.cfg_addr_i = 0;
  d.cfg_data_i = 0;
  d.v_valid_i = 0;
  d.r_ready_i = 0;
  d.n_x_i = 0; d.n_y_i = 0; d.n_z_i = 0;
  d.n_mag_valid_i = 0; d.n_mag_i = 0;
  d.n_degenerate_i = 0; d.n_profile_i = 0;
  d.n_lights_i = 0; d.n_src_id_i = 0;
  d.eval();
  for (int i = 0; i < 3; ++i) tk(d);
  d.rst_n = 1;
  d.eval();
  tk(d);
}

void wait_idle(Vzhao_light_stream& d) {
  int guard = 0;
  d.r_ready_i = 1;
  d.eval();
  while (!d.idle_o) {
    tk(d);
    if (++guard > 100000) {
      check(false, "the service never went idle", 1, 0);
      zhao::exit_hard(zhao::report_and_exit("light_stream_directed"));
    }
  }
  d.r_ready_i = 0;
  d.eval();
}

void cfg_write(Vzhao_light_stream& d, uint32_t light, uint32_t half, uint32_t word,
               uint32_t data) {
  d.cfg_we_i = 1;
  d.cfg_addr_i = static_cast<uint8_t>(((light & 0xF) << 4) | ((half & 1) << 3) | (word & 7));
  d.cfg_data_i = data;
  tk(d);
  d.cfg_we_i = 0;
  d.cfg_addr_i = 0;
  d.cfg_data_i = 0;
}

void cfg_commit(Vzhao_light_stream& d) {
  d.cfg_commit_i = 1;
  tk(d);
  d.cfg_commit_i = 0;
  d.eval();
}

// Half B is a 128-bit record written as four whole 32-bit bank words. The
// coefficients are NOT written as six semantic fields: the physical bank is
// four 32-bit words and a 20-bit field straddles them, which is exactly why
// GAINW is pinned rather than widened.
void pack_halfB(const Light& L, uint32_t w[4]) {
  const uint64_t lo = (static_cast<uint64_t>(L.cr) & 0xFFFFFull) |
                      ((static_cast<uint64_t>(L.cg) & 0xFFFFFull) << 20) |
                      ((static_cast<uint64_t>(L.cb) & 0xFFFFFull) << 40);
  const uint64_t hi = (static_cast<uint64_t>(L.er) & 0xFFFFFull) |
                      ((static_cast<uint64_t>(L.eg) & 0xFFFFFull) << 20) |
                      ((static_cast<uint64_t>(L.eb) & 0xFFFFFull) << 40);
  // bits [119:0] = {eb,eg,er,cb,cg,cr}; lo holds [59:0], hi holds [119:60]
  const uint64_t b0 = lo & 0xFFFFFFFFull;
  const uint64_t b1 = ((lo >> 32) & 0x0FFFFFFFull) | ((hi & 0xFull) << 28);
  const uint64_t b2 = (hi >> 4) & 0xFFFFFFFFull;
  const uint64_t b3 = (hi >> 36) & 0xFFFFFFFFull;
  w[0] = static_cast<uint32_t>(b0);
  w[1] = static_cast<uint32_t>(b1);
  w[2] = static_cast<uint32_t>(b2);
  w[3] = static_cast<uint32_t>(b3);
}

void load_light(Vzhao_light_stream& d, uint32_t idx, const Light& L) {
  cfg_write(d, idx, 0, 0, static_cast<uint32_t>(L.lx));
  cfg_write(d, idx, 0, 1, static_cast<uint32_t>(L.ly));
  cfg_write(d, idx, 0, 2, static_cast<uint32_t>(L.lz));
  cfg_write(d, idx, 0, 3, static_cast<uint32_t>(L.detail));
  uint32_t w[4];
  pack_halfB(L, w);
  for (uint32_t i = 0; i < 4; ++i) cfg_write(d, idx, 1, i, w[i]);
}

void load_env(Vzhao_light_stream& d, const Env& e) {
  cfg_write(d, kEnvIdx, 0, 0, e.ar);
  cfg_write(d, kEnvIdx, 0, 1, e.ag);
  cfg_write(d, kEnvIdx, 0, 2, e.ab);
  cfg_write(d, kEnvIdx, 0, 3, e.sr);
  cfg_write(d, kEnvIdx, 0, 4, e.sg);
  cfg_write(d, kEnvIdx, 0, 5, e.sb);
}

// A light set is published atomically. The block refuses a write that would
// disturb a generation with live terms, so publishing means draining first --
// which is the pinning rule doing its job, not a testbench convenience.
void publish(Vzhao_light_stream& d, const std::vector<Light>& L, uint32_t n, const Env& e) {
  wait_idle(d);
  for (uint32_t i = 0; i < n; ++i) load_light(d, i, L[i]);
  load_env(d, e);
  cfg_commit(d);
}

// ---------------------------------------------------------------------------
// THE COMPILED LAWS, called, never restated.
// ---------------------------------------------------------------------------
int32_t law_raw(int32_t nx, int32_t ny, int32_t nz, int32_t lx, int32_t ly, int32_t lz) {
  return zref::render::shade_from_world_normal_unclamped(nx, ny, nz, lx, ly, lz, nullptr);
}

uint64_t mag_of(int32_t nx, int32_t ny, int32_t nz) {
  const uint64_t s = static_cast<uint64_t>(nx) * static_cast<uint64_t>(nx) +
                     static_cast<uint64_t>(ny) * static_cast<uint64_t>(ny) +
                     static_cast<uint64_t>(nz) * static_cast<uint64_t>(nz);
  return zref::isqrt_u64(s);
}

uint32_t clamp01(int64_t v) {
  if (v <= 0) return 0;
  if (v >= kOne) return static_cast<uint32_t>(kOne);
  return static_cast<uint32_t>(v);
}

uint32_t rhu16(uint64_t p) { return static_cast<uint32_t>((p + 32768u) >> 16); }

// The FOLD is the ruled prose, not an oracle (GEOM.LIGHT.md: the vertex-RGB
// reference is unwritten). It calls the compiled law for the only part that
// has one.
struct Expect {
  Rgb rgb;
  bool degen = false;
  bool sat = false;
  uint32_t terms = 0, nulls = 0, lo = 0, hi = 0, raw_sat = 0, degen_terms = 0;
};

Expect expect_of(const Vtx& v, const std::vector<Light>& L, const Env& e) {
  Expect x;
  const uint64_t mag = v.mag_valid ? v.mag : mag_of(v.nx, v.ny, v.nz);
  x.degen = (mag == 0);
  uint64_t acc[3] = {0, 0, 0};
  const uint32_t nl = (v.nlights > kLightsMax) ? kLightsMax : v.nlights;
  if (nl == 0) {
    x.nulls = 1;
    x.terms = 1;
  }
  for (uint32_t i = 0; i < nl; ++i) {
    const Light& Li = L[i];
    x.terms++;
    uint32_t ndl;
    if (mag == 0) {
      ndl = 0;
      x.degen_terms++;
    } else if (v.creature) {
      // THE CREATURE ORACLE, compiled. The tuple is the producer's own pair.
      const int64_t n64[3] = {v.nx, v.ny, v.nz};
      ndl = static_cast<uint32_t>(zref::creature::lambert_from_world_normal(
          n64, static_cast<int64_t>(mag), Li.lx, Li.ly, Li.lz));
      const int32_t raw = law_raw(v.nx, v.ny, v.nz, Li.lx, Li.ly, Li.lz);
      if (raw < 0) x.lo++;
      if (raw > kOne) x.hi++;
      if (raw == INT32_MAX || raw == INT32_MIN) x.raw_sat++;
    } else {
      const int32_t raw = law_raw(v.nx, v.ny, v.nz, Li.lx, Li.ly, Li.lz);
      const int64_t sum = static_cast<int64_t>(raw) + static_cast<int64_t>(Li.detail);
      if (sum < 0) x.lo++;
      if (sum > kOne) x.hi++;
      if (raw == INT32_MAX || raw == INT32_MIN) x.raw_sat++;
      ndl = clamp01(sum);
    }
    const uint32_t g[6] = {Li.cr, Li.cg, Li.cb, Li.er, Li.eg, Li.eb};
    // SEPARATELY ROUNDED, one product at a time. Fusing gain and emission
    // before the multiply changes the answer and section 5 pins the pair.
    for (int k = 0; k < 6; ++k) acc[k % 3] += rhu16(static_cast<uint64_t>(g[k]) * ndl);
  }
  const uint32_t amb[3] = {e.ar, e.ag, e.ab};
  const uint32_t spl[3] = {e.sr, e.sg, e.sb};
  uint32_t out[3];
  for (int c = 0; c < 3; ++c) {
    const uint64_t t = acc[c] + amb[c] + spl[c];
    if (t > static_cast<uint64_t>(kOne)) x.sat = true;
    out[c] = (t > static_cast<uint64_t>(kOne)) ? static_cast<uint32_t>(kOne)
                                               : static_cast<uint32_t>(t);
  }
  x.rgb = {out[0], out[1], out[2]};
  return x;
}

// ---------------------------------------------------------------------------
// Feed a batch and collect every packet, in order. Accept and emit cycles are
// recorded separately so the INITIATION INTERVAL is measured rather than
// inferred from a latency.
// ---------------------------------------------------------------------------
struct RunStats {
  uint64_t first_cycle = 0, last_cycle = 0, clocks = 0;
  uint64_t first_emit_latency = 0;
};

std::vector<Out> run_batch(Vzhao_light_stream& d, const std::vector<Vtx>& vs, int stall_mod,
                           RunStats* st) {
  std::vector<Out> out;
  out.reserve(vs.size());
  size_t sent = 0;
  uint64_t guard = 0;
  const uint64_t t0 = g_cycles;
  uint64_t first_accept = 0;
  bool have_first = false;
  while (out.size() < vs.size()) {
    // stall_mod > 0 : the consumer refuses one cycle in stall_mod (a light stall)
    // stall_mod < 0 : the consumer ACCEPTS one cycle in -stall_mod (a heavy one).
    // Both are needed: a light stall proves values do not move, and only a
    // heavy one can fill the output queue and reach the fold's stall path.
    const bool ready =
        (stall_mod == 0)
            ? true
            : (stall_mod > 0 ? ((g_cycles % static_cast<uint64_t>(stall_mod)) != 0)
                             : ((g_cycles % static_cast<uint64_t>(-stall_mod)) == 0));
    d.r_ready_i = ready ? 1 : 0;
    if (sent < vs.size()) {
      const Vtx& v = vs[sent];
      d.v_valid_i = 1;
      d.n_x_i = static_cast<uint32_t>(v.nx);
      d.n_y_i = static_cast<uint32_t>(v.ny);
      d.n_z_i = static_cast<uint32_t>(v.nz);
      d.n_mag_valid_i = v.mag_valid ? 1 : 0;
      d.n_mag_i = v.mag;
      d.n_degenerate_i = v.producer_degen ? 1 : 0;
      d.n_profile_i = v.creature ? 1 : 0;
      d.n_lights_i = static_cast<uint8_t>(v.nlights & 0xF);
      d.n_src_id_i = v.src;
    } else {
      d.v_valid_i = 0;
    }
    d.eval();
    const bool acc = (sent < vs.size()) && d.v_ready_o;
    const bool emit = d.r_valid_o && ready;
    Out o{};
    if (emit) {
      o.rgb = {d.rgb_r_o, d.rgb_g_o, d.rgb_b_o};
      o.degen = d.degenerate_vtx_o != 0;
      o.src = static_cast<uint16_t>(d.src_id_o);
    }
    const uint64_t now = g_cycles;
    tk(d);
    if (acc) {
      if (!have_first) { first_accept = now; have_first = true; }
      ++sent;
    }
    if (emit) {
      if (out.empty() && st) st->first_emit_latency = now - first_accept;
      out.push_back(o);
    }
    if (++guard > 40000000ull) {
      // A hang must say WHERE it stopped. A bare "never completed" sends the
      // next reader to instrument the thing from scratch.
      std::printf(
          "[stream] HANG: sent=%llu out=%llu | vready=%d rvalid=%d idle=%d | "
          "normals=%u terms acc/ret=%u/%u roots iss/ret=%u/%u | waits nq=%u desc=%u "
          "div=%u col=%u out=%u\n",
          static_cast<unsigned long long>(sent), static_cast<unsigned long long>(out.size()),
          d.v_ready_o, d.r_valid_o, d.idle_o, d.normal_inputs_o, d.terms_accepted_o,
          d.terms_retired_o, d.roots_issued_o, d.roots_retired_o, d.normal_queue_wait_o,
          d.descriptor_wait_o, d.divider_backpressure_o, d.colour_backpressure_o,
          d.output_backpressure_o);
      check(false, "the batch never completed", 1, 0);
      zhao::exit_hard(zhao::report_and_exit("light_stream_directed"));
    }
  }
  d.v_valid_i = 0;
  d.r_ready_i = 0;
  d.eval();
  if (st) { st->first_cycle = t0; st->last_cycle = g_cycles; st->clocks = g_cycles - t0; }
  return out;
}

struct Tally {
  uint64_t normals = 0, supplied = 0, terms = 0, nulls = 0, vertices = 0, degen = 0;
  uint64_t lo = 0, hi = 0, sat = 0, raw_sat = 0, degen_terms = 0, seam = 0, nl_clamped = 0;
} g_t;

Out check_one(Vzhao_light_stream& d, const Vtx& v, const std::vector<Light>& L, const Env& e,
              const char* what, int stall_mod = 0, int32_t inject = 0) {
  const Expect x = expect_of(v, L, e);
  std::vector<Vtx> one{v};
  const std::vector<Out> got = run_batch(d, one, stall_mod, nullptr);
  check(got.size() == 1, "exactly one packet came out", 1, got.size());
  const uint32_t er = x.rgb.r + static_cast<uint32_t>(inject);
  check(got[0].rgb.r == er, what, er, got[0].rgb.r);
  check(got[0].rgb.g == x.rgb.g, "green channel", x.rgb.g, got[0].rgb.g);
  check(got[0].rgb.b == x.rgb.b, "blue channel", x.rgb.b, got[0].rgb.b);
  check(got[0].degen == x.degen, "degenerate_vtx_o rides the packet", x.degen ? 1 : 0,
        got[0].degen ? 1 : 0);
  check(got[0].src == v.src, "src_id rides the vertex", v.src, got[0].src);

  g_t.normals++;
  if (v.mag_valid) g_t.supplied++;
  g_t.terms += x.terms;
  g_t.nulls += x.nulls;
  g_t.vertices++;
  if (x.degen) g_t.degen++;
  g_t.lo += x.lo;
  g_t.hi += x.hi;
  if (x.sat) g_t.sat++;
  g_t.raw_sat += x.raw_sat;
  g_t.degen_terms += x.degen_terms;
  if (v.producer_degen != x.degen) g_t.seam++;
  if (v.nlights > kLightsMax) g_t.nl_clamped++;
  return got[0];
}

uint64_t g_rng = 0x8EBC6AF09C88C6E3ULL;
uint64_t rnd() {
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 7;
  g_rng ^= g_rng << 17;
  return g_rng;
}
int32_t rnd_biased() {
  switch (rnd() & 3) {
    case 0: return static_cast<int32_t>(rnd() & 7) - 3;
    case 1: return static_cast<int32_t>(rnd()) >> 16;
    case 2: return (rnd() & 1) ? INT32_MAX - static_cast<int32_t>(rnd() & 3)
                               : INT32_MIN + static_cast<int32_t>(rnd() & 3);
    default: return static_cast<int32_t>(rnd());
  }
}
// The creature producer range-reduces until every lane fits |n| <= 2^30, so
// its tuples are sampled from THAT domain rather than from all of s32.
int32_t rnd_skin_lane() {
  switch (rnd() & 3) {
    case 0: return static_cast<int32_t>(rnd() & 7) - 3;
    case 1: return -(1 << 30);                       // the exact shift boundary
    case 2: return static_cast<int32_t>(rnd() % 131073) - 65536;
    default: return (static_cast<int32_t>(rnd()) % (1 << 30));
  }
}

}  // namespace

int main(int argc, char** argv) {
  bool break_oracle = false;
  for (int i = 1; i < argc; ++i)
    if (!std::strcmp(argv[i], "--break-oracle")) break_oracle = true;

  Verilated::commandArgs(argc, argv);
  Vzhao_light_stream* dutp = new Vzhao_light_stream;  // heap, harness rule
  Vzhao_light_stream& dut = *dutp;
  reset_dut(dut);

  std::vector<Light> L(kLightsMax);
  Env env;

  // ---- 1: OUT OF RESET -----------------------------------------------------
  {
    check(dut.idle_o == 1, "idle out of reset", 1, dut.idle_o);
    check(dut.v_ready_o == 1, "the arena accepts immediately: no cold table fill", 1,
          dut.v_ready_o);
    check(dut.r_valid_o == 0, "no packet offered out of reset", 0, dut.r_valid_o);
    check(dut.cfg_gen_o == 0, "generation 0 is active out of reset", 0, dut.cfg_gen_o);
  }

  // ---- 2: HAND-COMPUTED, ONE LIGHT ----------------------------------------
  {
    L[0] = Light{}; L[0].ly = kOne; L[0].cr = 0x10000; L[0].cg = 0x8000; L[0].cb = 0x4000;
    publish(dut, L, 1, env);
    check(dut.cfg_gen_o == 1, "the commit published the shadow generation", 1, dut.cfg_gen_o);
    check(law_raw(0, kOne, 0, 0, kOne, 0) == kOne,
          "the compiled law: +Y normal, overhead sun is exactly 1.0",
          static_cast<uint32_t>(kOne), static_cast<uint32_t>(law_raw(0, kOne, 0, 0, kOne, 0)));
    Vtx v; v.ny = kOne; v.nlights = 1; v.src = 0x0201;
    const Out o = check_one(dut, v, L, env, "one light, unit gain");
    check(o.rgb.r == 0x10000u, "hand-computed red  == 0x10000", 0x10000u, o.rgb.r);
    check(o.rgb.g == 0x08000u, "hand-computed green == 0x8000", 0x08000u, o.rgb.g);
    check(o.rgb.b == 0x04000u, "hand-computed blue  == 0x4000", 0x04000u, o.rgb.b);
  }

  // ---- 3: THE CLAMP IS ONCE PER LIGHT --------------------------------------
  // A second INDEPENDENT sun behind the surface contributes zero and must not
  // SUBTRACT what the first gave. Clamping once per vertex would return black.
  {
    const uint32_t lo0 = dut.ndl_clamp_lo_o;
    L[0] = Light{}; L[0].ly = kOne;  L[0].cr = 0x8000; L[0].cg = 0x8000; L[0].cb = 0x8000;
    L[1] = Light{}; L[1].ly = -kOne; L[1].cr = 0x10000; L[1].cg = 0x10000; L[1].cb = 0x10000;
    publish(dut, L, 2, env);
    Vtx v; v.ny = kOne; v.nlights = 2; v.src = 0x0301;
    const Out o = check_one(dut, v, L, env, "two lights, one turned away");
    check(o.rgb.r == 0x8000u,
          "D-1: a second sun's negative dot does NOT subtract the first sun's light", 0x8000u,
          o.rgb.r);
    check(dut.ndl_clamp_lo_o == lo0 + 1, "ndl_clamp_lo_o moved by exactly 1 across this case",
          lo0 + 1, dut.ndl_clamp_lo_o);
  }

  // ---- 4: DETAIL IS INSIDE THE RENDER CLAMP, AND ABSENT FROM THE CREATURE --
  // The two profiles differ here and only here. `clamp01(raw + detail)` is not
  // `clamp01(raw) + detail`: raw = -0.125, detail = +0.25 must LIGHT the face
  // on the render path. The creature profile admits no detail term at all, and
  // the SAME descriptor must therefore produce a DIFFERENT answer -- which is
  // the check that proves the profile bit is load-bearing rather than decorative.
  {
    const uint32_t hi0 = dut.ndl_clamp_hi_o;
    L[0] = Light{}; L[0].ly = -kOne / 8; L[0].detail = kOne / 4;
    L[0].cr = 0x10000;
    publish(dut, L, 1, env);

    Vtx vr; vr.ny = kOne; vr.nlights = 1; vr.src = 0x0401;
    const Out r = check_one(dut, vr, L, env, "detail inside the clamp rescues a turned face");
    check(r.rgb.r == 0x2000u, "hand-computed: clamp01(-0.125 + 0.25) * 1.0 == 0x2000", 0x2000u,
          r.rgb.r);

    Vtx vc; vc.ny = kOne; vc.nlights = 1; vc.src = 0x0402;
    vc.creature = true; vc.mag_valid = true; vc.mag = static_cast<uint32_t>(mag_of(0, kOne, 0));
    const Out c = check_one(dut, vc, L, env, "the creature profile admits NO detail term");
    check(c.rgb.r == 0u, "the same descriptor on the creature profile stays dark", 0u, c.rgb.r);
    check(r.rgb.r != c.rgb.r, "the profile bit changes the answer: it is load-bearing", 1,
          (r.rgb.r != c.rgb.r) ? 1 : 0);

    // detail past full scale on the render path
    L[0].ly = kOne; L[0].detail = kOne;
    publish(dut, L, 1, env);
    Vtx vh; vh.ny = kOne; vh.nlights = 1; vh.src = 0x0403;
    const Out h = check_one(dut, vh, L, env, "detail past full scale clamps at 1.0");
    check(h.rgb.r == 0x10000u, "hand-computed: clamp01(1.0 + 1.0) == 0x10000", 0x10000u, h.rgb.r);
    check(dut.ndl_clamp_hi_o == hi0 + 1, "ndl_clamp_hi_o moved by exactly 1 across this case",
          hi0 + 1, dut.ndl_clamp_hi_o);
  }

  // ---- 5: THE TRAP -- GAIN AND EMISSION ARE NOT COMBINED BEFORE THE PRODUCT
  // Response exactly 32768 (= 0.5), gain 1 and emission 1, both in raw units.
  //   separately rounded : rhu(1*32768) + rhu(1*32768) = 1 + 1 = 2
  //   fused before round : rhu(2*32768)               =         1
  // The whole reason the existing per-product rounding is preserved.
  {
    L[0] = Light{};
    L[0].ly = 0x8000;      // half-strength sun on a unit +Y normal -> raw = 32768
    L[0].cr = 1;
    L[0].er = 1;
    publish(dut, L, 1, env);
    const int32_t raw = law_raw(0, kOne, 0, 0, 0x8000, 0);
    check(raw == 32768, "the compiled law really does return exactly 32768 here", 32768,
          static_cast<uint32_t>(raw));
    Vtx v; v.ny = kOne; v.nlights = 1; v.src = 0x0501;
    const Out o = check_one(dut, v, L, env, "gain and emission are rounded SEPARATELY");
    check(o.rgb.r == 2u,
          "TRAP: separately rounded terms sum to 2; a fused product would give 1", 2u, o.rgb.r);
  }

  // ---- 6: ONE SATURATE, AT THE END -----------------------------------------
  {
    const uint32_t sat0 = dut.rgb_sat_o;
    const uint32_t gr = 0x10000 / 5, gg = 0x10000 / 20;
    for (uint32_t i = 0; i < kLightsMax; ++i) {
      L[i] = Light{}; L[i].ly = kOne; L[i].cr = gr; L[i].cg = gg; L[i].cb = 0;
    }
    publish(dut, L, kLightsMax, env);
    Vtx v; v.ny = kOne; v.nlights = kLightsMax; v.src = 0x0601;
    const Out o = check_one(dut, v, L, env, "eight lights: red saturates, green does not");
    check(o.rgb.r == 0x10000u, "red lands exactly on the 1.0 rail", 0x10000u, o.rgb.r);
    check(o.rgb.g == 8u * gg, "green lands exactly on its unclipped sum (no early clip, no wrap)",
          8u * gg, o.rgb.g);
    check(dut.rgb_sat_o == sat0 + 1, "rgb_sat_o moved by exactly 1 across this case", sat0 + 1,
          dut.rgb_sat_o);
  }

  // ---- 7: AMBIENT, SPILL, AND THE ZERO-LIGHT VERTEX ------------------------
  {
    env.ar = 0x2000; env.ag = 0x1000; env.ab = 0x0800;
    env.sr = 0x1000; env.sg = 0; env.sb = 0;
    L[0] = Light{}; L[0].ly = -kOne; L[0].cr = 0x10000; L[0].cg = 0x10000; L[0].cb = 0x10000;
    publish(dut, L, 1, env);
    Vtx v; v.ny = kOne; v.nlights = 1; v.src = 0x0701;
    const Out o = check_one(dut, v, L, env, "an unlit face still receives ambient and spill");
    check(o.rgb.r == 0x3000u, "hand-computed: ambient 0x2000 + spill 0x1000", 0x3000u, o.rgb.r);

    const uint32_t n0 = dut.terms_null_o;
    Vtx z; z.ny = kOne; z.nlights = 0; z.src = 0x0702;
    const Out oz = check_one(dut, z, L, env, "nlights==0 emits ambient+spill only");
    check(oz.rgb.r == 0x3000u, "environment-only red", 0x3000u, oz.rgb.r);
    // A zero-light vertex still owes an in-order packet, so it spends ONE term
    // slot whose response is forced to zero. Counted separately, because a
    // null term dressed as light work would inflate the throughput evidence.
    check(dut.terms_null_o == n0 + 1, "terms_null_o moved by exactly 1, and it is NOT light work",
          n0 + 1, dut.terms_null_o);
  }

  // ---- 8: THE DEGENERATE VERTEX AND THE SEAM DETECTOR ----------------------
  {
    const uint32_t d0 = dut.degenerate_o;
    const uint32_t s0 = dut.seam_mismatch_o;
    const uint32_t dt0 = dut.degenerate_terms_o;
    L[0] = Light{}; L[0].ly = kOne; L[0].detail = kOne;
    L[0].cr = 0x10000; L[0].cg = 0x10000; L[0].cb = 0x10000;
    publish(dut, L, 1, env);

    Vtx v; v.nx = 0; v.ny = 0; v.nz = 0; v.producer_degen = true; v.nlights = 1; v.src = 0x0801;
    const Out o = check_one(dut, v, L, env, "degenerate normal: shade 0, ambient only");
    check(o.rgb.r == 0x3000u, "a directionless vertex gets environment and no light term",
          0x3000u, o.rgb.r);
    check(o.degen == true, "degenerate_vtx_o rides the packet", 1, o.degen ? 1 : 0);
    check(dut.degenerate_o == d0 + 1, "degenerate_o moved by exactly 1", d0 + 1,
          dut.degenerate_o);
    check(dut.degenerate_terms_o == dt0 + 1,
          "degenerate_terms_o counts the TERM, not the normal: magnitude reuse must not "
          "divide the evidence by K", dt0 + 1, dut.degenerate_terms_o);
    check(dut.seam_mismatch_o == s0, "seam quiet when the producer agrees with the law", s0,
          dut.seam_mismatch_o);

    // Now fire it, in both directions. The two operands arrive by INDEPENDENT
    // paths -- the producer's flag on the port, the magnitude from the block's
    // own root -- which is the only reason it can fire at all.
    Vtx a; a.ny = kOne; a.producer_degen = true; a.nlights = 1; a.src = 0x0802;
    check_one(dut, a, L, env, "producer says degenerate, normal is real: the law wins");
    check(dut.seam_mismatch_o == s0 + 1, "seam_mismatch_o fired on flag-high/normal-real", s0 + 1,
          dut.seam_mismatch_o);
    Vtx b; b.producer_degen = false; b.nlights = 1; b.src = 0x0803;
    check_one(dut, b, L, env, "producer says fine, normal is zero: the law wins");
    check(dut.seam_mismatch_o == s0 + 2, "seam_mismatch_o fired on flag-low/normal-zero", s0 + 2,
          dut.seam_mismatch_o);
  }

  // ---- 9: THE EPOCH IS PINNED, AND THE REFUSAL IS COUNTED ------------------
  // "Latest descriptor" is not a valid identity for an in-flight light term.
  // Offering a vertex, then trying to rewrite the generation it is using while
  // it is still in flight, must be REFUSED -- not applied, not queued.
  {
    const uint32_t e0 = dut.epoch_refusals_o;
    const uint32_t c0 = dut.cfg_refused_o;
    L[0] = Light{}; L[0].ly = kOne; L[0].cr = 0x8000;
    publish(dut, L, 1, env);

    // Put terms ACTUALLY in flight, then attack the generation they hold.
    // Offering a vertex is not enough: the normal has to be prepared and the
    // first term issued before any reference exists, so the stimulus waits for
    // `terms_accepted_o` to move rather than for a fixed number of clocks. A
    // fixed wait here is how a refusal test quietly stops reaching the state it
    // is about, and then reads as a pass because nothing else changed.
    const uint32_t ta0 = dut.terms_accepted_o;
    dut.v_valid_i = 1;
    dut.n_x_i = 0; dut.n_y_i = static_cast<uint32_t>(kOne); dut.n_z_i = 0;
    dut.n_mag_valid_i = 0; dut.n_degenerate_i = 0; dut.n_profile_i = 0;
    dut.n_lights_i = 8; dut.n_src_id_i = 0x0901;
    for (int i = 0; i < 4; ++i) tk(dut);
    dut.v_valid_i = 0;
    dut.eval();
    int spin = 0;
    while (dut.terms_accepted_o == ta0 && spin < 2000) { tk(dut); ++spin; }
    check(dut.terms_accepted_o > ta0, "the refusal test reached the state it is about", 1,
          (dut.terms_accepted_o > ta0) ? 1 : 0);

    // Terms are now live on the ACTIVE generation. Writing the SHADOW at this
    // moment is LEGAL and must not be refused -- that is the entire point of
    // double buffering, and a test that expected a refusal here would be
    // asserting the absence of the feature.
    const uint32_t gen_before = dut.cfg_gen_o;
    cfg_write(dut, 0, 0, 0, 0x00001234);
    check(dut.epoch_refusals_o == e0,
          "writing the SHADOW while the ACTIVE set has live terms is legal, not refused", e0,
          dut.epoch_refusals_o);

    // Publishing that shadow is also legal. What it does is make the OLD
    // active -- still referenced by every in-flight term -- the new shadow.
    cfg_commit(dut);
    check(dut.cfg_gen_o != gen_before, "the commit published", 1,
          (dut.cfg_gen_o != gen_before) ? 1 : 0);
    check(dut.epoch_refusals_o == e0, "and the commit itself was not refused", e0,
          dut.epoch_refusals_o);

    // NOW the shadow is pinned. A write into it would mutate the descriptor an
    // in-flight term is still going to read, and "latest descriptor" is not a
    // valid identity for that term. It must be REFUSED, not queued, not applied.
    cfg_write(dut, 0, 0, 0, 0xDEADBEEF);
    check(dut.epoch_refusals_o == e0 + 1, "a write into the PINNED generation is REFUSED", e0 + 1,
          dut.epoch_refusals_o);
    // And a second commit would hand those live terms' set back as the active
    // one while they are still draining from it.
    cfg_commit(dut);
    check(dut.epoch_refusals_o == e0 + 2, "a commit that would unpin a live generation is REFUSED",
          e0 + 2, dut.epoch_refusals_o);
    wait_idle(dut);

    // And the map still refuses what is simply not in it.
    cfg_write(dut, kLightsMax, 0, 0, 0xDEADBEEF);       // light past the set
    cfg_write(dut, 0, 0, 4, 0xDEADBEEF);                // word past the half
    cfg_write(dut, kEnvIdx, 0, 6, 0xDEADBEEF);          // word past the environment
    cfg_write(dut, kEnvIdx, 1, 0, 0xDEADBEEF);          // the environment has no half B
    check(dut.cfg_refused_o == c0 + 4, "cfg_refused_o moved by exactly 4 across this case",
          c0 + 4, dut.cfg_refused_o);
    // The refusals did not alias onto light 0: republish the set cleanly and
    // light a vertex. The counter is not the check -- a masked index would
    // fold an illegal write onto light 0 in silence and still count nothing.
    publish(dut, L, 1, env);
    Vtx v; v.ny = kOne; v.nlights = 1; v.src = 0x0902;
    const Out o = check_one(dut, v, L, env, "light 0 is untouched by the refused writes");
    check(o.rgb.r == 0x8000u + env.ar + env.sr, "the refused writes changed nothing",
          0x8000u + env.ar + env.sr, o.rgb.r);
  }

  // ---- 10: THE RENDER DIFFERENTIAL -----------------------------------------
  int cov_lit = 0, cov_dark = 0, cov_full = 0;
  {
    Env zero;
    std::vector<Vtx> vs;
    std::vector<uint32_t> want;
    for (int i = 0; i < 400; ++i) {
      Light d;
      switch (rnd() & 3) {
        case 0: d.lx = KLX; d.ly = KLY; d.lz = KLZ; break;
        case 1: d.lx = static_cast<int32_t>(rnd()); d.ly = static_cast<int32_t>(rnd());
                d.lz = static_cast<int32_t>(rnd()); break;
        default: d.lx = static_cast<int32_t>(rnd() % 131073) - 65536;
                 d.ly = static_cast<int32_t>(rnd() % 131073) - 65536;
                 d.lz = static_cast<int32_t>(rnd() % 131073) - 65536; break;
      }
      d.cr = 0x10000;
      L[0] = d;
      publish(dut, L, 1, zero);

      Vtx v;
      v.nx = rnd_biased(); v.ny = rnd_biased(); v.nz = rnd_biased();
      if ((rnd() % 29) == 0) { v.nx = 0; v.ny = 0; v.nz = 0; }
      v.producer_degen = (v.nx == 0 && v.ny == 0 && v.nz == 0);
      v.nlights = 1;
      v.src = static_cast<uint16_t>(rnd() & 0xFFFF);
      const int32_t raw = law_raw(v.nx, v.ny, v.nz, d.lx, d.ly, d.lz);
      const uint32_t w = v.producer_degen ? 0u : clamp01(raw);
      if (w == 0) cov_dark++;
      else if (w == static_cast<uint32_t>(kOne)) cov_full++;
      else cov_lit++;
      const int32_t inject = (break_oracle && i == 3) ? 1 : 0;
      const Out o = check_one(dut, v, L, zero,
                              "DIFFERENTIAL(render): rgb_r == clamp01(shade_from_world_normal)",
                              (i % 23 == 0) ? 3 : 0, inject);
      check(o.rgb.r == w + static_cast<uint32_t>(inject), "render differential value",
            w + static_cast<uint32_t>(inject), o.rgb.r);
    }
    check(cov_lit > 20, "coverage: partially lit render points", 20,
          static_cast<uint32_t>(cov_lit));
    check(cov_dark > 20, "coverage: dark render points", 20, static_cast<uint32_t>(cov_dark));
    check(cov_full > 5, "coverage: fully lit render points", 5, static_cast<uint32_t>(cov_full));
  }

  // ---- 11: THE CREATURE DIFFERENTIAL, AGAINST ITS OWN COMPILED ORACLE ------
  // The producer's exact {direction, magnitude} pair, NOT recomputed and NOT
  // rounded into a unit vector. The expectation is
  // `zref::creature::lambert_from_world_normal` -- the creature's own law,
  // not the renderer's swapped in for it.
  int ccov_lit = 0, ccov_dark = 0, ccov_full = 0;
  {
    Env zero;
    for (int i = 0; i < 400; ++i) {
      Light d;
      d.lx = static_cast<int32_t>(rnd() % 131073) - 65536;
      d.ly = static_cast<int32_t>(rnd() % 131073) - 65536;
      d.lz = static_cast<int32_t>(rnd() % 131073) - 65536;
      if ((rnd() & 3) == 0) { d.lx = KLX; d.ly = KLY; d.lz = KLZ; }
      d.cr = 0x10000;
      // A detail that must be IGNORED on this profile; a build that admitted
      // it would fail here and nowhere else.
      d.detail = static_cast<int32_t>(rnd() % 131073) - 65536;
      L[0] = d;
      publish(dut, L, 1, zero);

      Vtx v;
      v.nx = rnd_skin_lane(); v.ny = rnd_skin_lane(); v.nz = rnd_skin_lane();
      if ((rnd() % 29) == 0) { v.nx = 0; v.ny = 0; v.nz = 0; }
      const uint64_t mag = mag_of(v.nx, v.ny, v.nz);
      v.mag_valid = true;
      v.mag = static_cast<uint32_t>(mag);
      v.creature = true;
      v.producer_degen = (mag == 0);
      v.nlights = 1;
      v.src = static_cast<uint16_t>(rnd() & 0xFFFF);

      const int64_t n64[3] = {v.nx, v.ny, v.nz};
      const uint32_t w = (mag == 0) ? 0u
                                    : static_cast<uint32_t>(zref::creature::lambert_from_world_normal(
                                          n64, static_cast<int64_t>(mag), d.lx, d.ly, d.lz));
      if (w == 0) ccov_dark++;
      else if (w == static_cast<uint32_t>(kOne)) ccov_full++;
      else ccov_lit++;
      const int32_t inject = (break_oracle && i == 9) ? 1 : 0;
      const Out o = check_one(dut, v, L, zero,
                              "DIFFERENTIAL(creature): rgb_r == lambert_from_world_normal",
                              (i % 19 == 0) ? 2 : 0, inject);
      check(o.rgb.r == w + static_cast<uint32_t>(inject), "creature differential value",
            w + static_cast<uint32_t>(inject), o.rgb.r);
    }
    check(ccov_lit > 20, "coverage: partially lit creature points", 20,
          static_cast<uint32_t>(ccov_lit));
    check(ccov_dark > 20, "coverage: dark creature points", 20,
          static_cast<uint32_t>(ccov_dark));
    check(ccov_full > 2, "coverage: fully lit creature points", 2,
          static_cast<uint32_t>(ccov_full));
    // NO NEW ROOTS on a fully prepared fixture: the supplied magnitude is used
    // as given. This is the whole point of the creature adapter and it is a
    // counter, not a comment.
    check(dut.supplied_mags_o >= 400u, "the creature fixture supplied its own magnitudes", 400,
          dut.supplied_mags_o);
  }

  // ---- 12: BACKPRESSURE IS TRANSPARENT -------------------------------------
  {
    L[0] = Light{}; L[0].lx = KLX; L[0].ly = KLY; L[0].lz = KLZ;
    L[0].cr = 0xC000; L[0].cg = 0x9000; L[0].cb = 0x3000;
    publish(dut, L, 1, env);
    Vtx v; v.nx = 3000; v.ny = 40000; v.nz = -9000; v.nlights = 1;
    v.src = 0x0C01;
    const Out a = check_one(dut, v, L, env, "key light, no stall", 0);
    v.src = 0x0C02;
    const Out b = check_one(dut, v, L, env, "key light, 1-in-3 stall", 3);
    v.src = 0x0C03;
    const Out c = check_one(dut, v, L, env, "key light, 1-in-2 stall", 2);
    check(a.rgb.r == b.rgb.r && b.rgb.r == c.rgb.r, "stalling does not move the answer (r)",
          a.rgb.r, c.rgb.r);
    check(a.rgb.g == b.rgb.g && b.rgb.g == c.rgb.g, "stalling does not move the answer (g)",
          a.rgb.g, c.rgb.g);
    check(a.rgb.b == b.rgb.b && b.rgb.b == c.rgb.b, "stalling does not move the answer (b)",
          a.rgb.b, c.rgb.b);

    // `output_backpressure_o` only fires when the output queue is FULL at the
    // moment a vertex would fold, which one vertex at a time can never reach.
    // A burst against a consumer taking one packet in four does reach it, and
    // the values must still be byte-identical to the unstalled run.
    const uint32_t ob0 = dut.output_backpressure_o;
    std::vector<Vtx> burst(64, v);
    for (uint32_t i = 0; i < burst.size(); ++i)
      burst[i].src = static_cast<uint16_t>(0x0D00 + i);
    const std::vector<Out> fast = run_batch(dut, burst, 0, nullptr);
    // One packet accepted every 24 clocks against a service producing one
    // every few: the queue fills, the fold stalls, and the machine must hold
    // rather than drop. A 1-in-4 stall never reaches this and reads as a pass.
    const std::vector<Out> slow = run_batch(dut, burst, -24, nullptr);
    uint32_t moved = 0;
    for (size_t i = 0; i < burst.size(); ++i) {
      if (fast[i].rgb.r != slow[i].rgb.r || fast[i].rgb.g != slow[i].rgb.g ||
          fast[i].rgb.b != slow[i].rgb.b || fast[i].src != slow[i].src)
        ++moved;
    }
    check(moved == 0, "a full output queue changed no packet and reordered none", 0, moved);
    check(dut.output_backpressure_o > ob0,
          "output_backpressure_o was SEEN TO MOVE: the fold really does stall rather than drop",
          1, (dut.output_backpressure_o > ob0) ? 1 : 0);
    g_t.normals += 2 * burst.size();
    g_t.vertices += 2 * burst.size();
    g_t.terms += 2 * burst.size();
  }

  // =========================================================================
  // 13: THE RULED FIXTURE -- 120,000 NORMALS, 480,000 LIGHT TERMS
  // =========================================================================
  // The stress profile exactly as ruled: every normal UNPREPARED (no cache-hit
  // assumption, so 120,000 real root jobs), four lights on every vertex, a
  // consumer that never stalls. Nothing here narrows the workload.
  uint64_t fix4_clocks = 0;
  {
    reset_dut(dut);
    g_t = Tally{};
    Env zero;
    for (uint32_t i = 0; i < 4; ++i) {
      L[i] = Light{};
      L[i].lx = KLX + static_cast<int32_t>(i) * 977;
      L[i].ly = KLY - static_cast<int32_t>(i) * 311;
      L[i].lz = KLZ + static_cast<int32_t>(i) * 53;
      L[i].detail = (i == 2) ? 512 : 0;
      L[i].cr = 0x4000; L[i].cg = 0x2000; L[i].cb = 0x1000;
      L[i].er = (i == 1) ? 0x0800 : 0;
    }
    publish(dut, L, 4, zero);

    constexpr uint32_t kN = 120000;
    std::vector<Vtx> vs(kN);
    std::vector<Expect> ex(kN);
    for (uint32_t i = 0; i < kN; ++i) {
      Vtx v;
      v.nx = static_cast<int32_t>(rnd() % 131073) - 65536;
      v.ny = static_cast<int32_t>(rnd() % 131073) - 65536;
      v.nz = static_cast<int32_t>(rnd() % 131073) - 65536;
      if ((i % 4001) == 0) { v.nx = 0; v.ny = 0; v.nz = 0; }
      v.producer_degen = (v.nx == 0 && v.ny == 0 && v.nz == 0);
      v.nlights = 4;
      v.src = static_cast<uint16_t>(i & 0xFFFF);
      vs[i] = v;
      ex[i] = expect_of(v, L, zero);
    }

    const uint32_t ri0 = dut.roots_issued_o;
    const uint32_t rr0 = dut.roots_retired_o;
    const uint32_t ta0 = dut.terms_accepted_o;
    const uint32_t tr0 = dut.terms_retired_o;
    const uint32_t dp0 = dut.dot_product_slots_o;
    const uint32_t sp0 = dut.square_product_slots_o;
    const uint32_t up0 = dut.unused_product_slots_o;

    RunStats st;
    const std::vector<Out> got = run_batch(dut, vs, 0, &st);
    fix4_clocks = st.clocks;

    check(got.size() == kN, "every vertex of the fixture emitted a packet", kN, got.size());
    uint32_t bad = 0;
    for (uint32_t i = 0; i < kN && i < got.size(); ++i) {
      if (got[i].rgb.r != ex[i].rgb.r || got[i].rgb.g != ex[i].rgb.g ||
          got[i].rgb.b != ex[i].rgb.b || got[i].src != vs[i].src) {
        if (bad < 5)
          check(false, "fixture packet matches the composed law", ex[i].rgb.r, got[i].rgb.r);
        ++bad;
      }
    }
    check(bad == 0, "all 120,000 fixture packets are exact and in order", 0, bad);

    const uint32_t roots = dut.roots_retired_o - rr0;
    const uint32_t terms = dut.terms_retired_o - tr0;
    check(dut.roots_issued_o - ri0 == kN,
          "root jobs are ONE PER NORMAL (120k), not one per term (480k)", kN,
          dut.roots_issued_o - ri0);
    check(roots == kN, "every root retired", kN, roots);
    check(terms == 4u * kN, "the fixture ran exactly 480,000 light terms", 4u * kN, terms);
    check(dut.terms_accepted_o - ta0 == 4u * kN, "accepted == retired: no term lost or reissued",
          4u * kN, dut.terms_accepted_o - ta0);

    // THE PRODUCT-SLOT CONSERVATION LAW. Two lanes, every clock, for the whole
    // run: dot + square + unused must equal 2 * clocks. A second engine's
    // products would not be in this budget, so cloning the scalar engine to
    // make the rate breaks this and nothing else would notice.
    const uint64_t dots = dut.dot_product_slots_o - dp0;
    const uint64_t sqs  = dut.square_product_slots_o - sp0;
    const uint64_t unu  = dut.unused_product_slots_o - up0;
    check(dots == 3ull * 4ull * kN, "three dot products per light term, exactly",
          3ull * 4ull * kN, dots);
    check(sqs == 3ull * kN, "three squares per normal, exactly", 3ull * kN, sqs);
    check(dots + sqs + unu == 2ull * st.clocks,
          "product-slot conservation: two lanes every clock, all accounted",
          2ull * st.clocks, dots + sqs + unu);

    const double ii = static_cast<double>(st.clocks) / static_cast<double>(terms);
    std::printf(
        "\n[stream] ===== THE RULED FIXTURE: 120,000 normals / 480,000 light terms =====\n"
        "[stream] clocks              = %llu\n"
        "[stream] II per light term   = %.4f clk\n"
        "[stream] first-emit latency  = %llu clk\n"
        "[stream] root jobs           = %u  (one per normal; 480,000 would be one per term)\n"
        "[stream] product slots       = %llu dot + %llu square + %llu unused = %llu (= 2 x clocks)\n"
        "[stream] waits               = normal-queue %u, descriptor %u, divider %u, colour %u, output %u\n"
        "[stream] gate <= 1,000,000   : %s\n"
        "[stream] 20%%-reserved envelope 1,333,333 : %s\n"
        "[stream] raw frame 1,666,666            : %s\n"
        "[stream] scalar baseline was 167.0 clk/term = 80,160,000 clk. Speedup = %.1fx\n\n",
        static_cast<unsigned long long>(st.clocks), ii,
        static_cast<unsigned long long>(st.first_emit_latency), roots,
        static_cast<unsigned long long>(dots), static_cast<unsigned long long>(sqs),
        static_cast<unsigned long long>(unu),
        static_cast<unsigned long long>(dots + sqs + unu),
        dut.normal_queue_wait_o, dut.descriptor_wait_o, dut.divider_backpressure_o,
        dut.colour_backpressure_o, dut.output_backpressure_o,
        (st.clocks <= 1000000ull) ? "PASS" : "FAIL",
        (st.clocks <= 1333333ull) ? "inside" : "OVER",
        (st.clocks <= 1666666ull) ? "inside" : "OVER",
        80160000.0 / static_cast<double>(st.clocks));

    check(st.clocks <= 1000000ull,
          "the ruled fixture meets the proposed <=1,000,000-clock lighting gate", 1000000ull,
          st.clocks);
  }

  // =========================================================================
  // 14: THE EIGHT-LIGHT WORKLOAD IS OVER BUDGET, AND SAYS SO
  // =========================================================================
  // 120,000 x 8 = 960,000 terms = 1,920,000 issue clocks at II2, which fails
  // even the unreserved 1,666,666-clock frame. This test asserts the OVERRUN.
  // It exists because "eight descriptors are supported" quietly standing in
  // for "eight lights meet rate" is how an admitted workload gets relabelled
  // instead of solved -- and a test that only ever proved things fit could
  // never catch that.
  {
    reset_dut(dut);
    Env zero;
    for (uint32_t i = 0; i < kLightsMax; ++i) {
      L[i] = Light{};
      L[i].lx = KLX + static_cast<int32_t>(i) * 131;
      L[i].ly = KLY; L[i].lz = KLZ;
      L[i].cr = 0x2000; L[i].cg = 0x1000; L[i].cb = 0x0800;
    }
    publish(dut, L, kLightsMax, zero);

    constexpr uint32_t kN = 120000;
    std::vector<Vtx> vs(kN);
    for (uint32_t i = 0; i < kN; ++i) {
      Vtx v;
      v.nx = static_cast<int32_t>(rnd() % 131073) - 65536;
      v.ny = static_cast<int32_t>(rnd() % 131073) - 65536;
      v.nz = static_cast<int32_t>(rnd() % 131073) - 65536;
      v.producer_degen = false;
      v.nlights = kLightsMax;
      v.src = static_cast<uint16_t>(i & 0xFFFF);
      vs[i] = v;
    }
    const uint32_t tr0 = dut.terms_retired_o;
    RunStats st;
    const std::vector<Out> got = run_batch(dut, vs, 0, &st);
    const uint32_t terms = dut.terms_retired_o - tr0;
    check(got.size() == kN, "the eight-light fixture emitted every packet", kN, got.size());
    check(terms == 8u * kN, "the eight-light fixture ran exactly 960,000 light terms", 8u * kN,
          terms);
    const double ii = static_cast<double>(st.clocks) / static_cast<double>(terms);
    std::printf(
        "[stream] ===== THE EIGHT-LIGHT WORKLOAD (a DIFFERENT workload) =====\n"
        "[stream] 120,000 normals x 8 lights = 960,000 terms\n"
        "[stream] clocks = %llu, II = %.4f clk/term\n"
        "[stream] against the raw 1,666,666-clock frame: %.2fx OVER. This is reported,\n"
        "[stream] not relabelled. Meeting it needs II1 (32 divider stages plus matching\n"
        "[stream] product and colour bandwidth) or a second qualified lane, and its cost.\n\n",
        static_cast<unsigned long long>(st.clocks), ii,
        static_cast<double>(st.clocks) / 1666666.0);
    check(st.clocks > 1666666ull,
          "EIGHT LIGHTS ON EVERY VERTEX IS OVER THE RAW FRAME -- asserted, not hidden",
          1666666ull, st.clocks);
    check(st.clocks > 1333333ull, "and over the reserved envelope too", 1333333ull, st.clocks);
  }

  // ---- 15: CONSERVATION AND THE COUNTERS THAT MUST STAY QUIET -------------
  {
    check(dut.tag_mismatch_o == 0,
          "tag_mismatch_o quiet: the divider's own tag agreed with the side channel on every "
          "term (fired deliberately in tests/mutants, not asserted zero on trust)",
          0, dut.tag_mismatch_o);
    check(dut.terms_accepted_o == dut.terms_retired_o,
          "every accepted term retired: accepted == retired, nothing lost, nothing reissued",
          dut.terms_accepted_o, dut.terms_retired_o);
    check(dut.roots_issued_o == dut.roots_retired_o, "every issued root retired",
          dut.roots_issued_o, dut.roots_retired_o);
    check(dut.normal_inputs_o == dut.vertices_lit_o,
          "every normal accepted produced exactly one lit vertex", dut.normal_inputs_o,
          dut.vertices_lit_o);
    check(dut.normal_prepared_o + dut.supplied_mags_o == dut.normal_inputs_o,
          "every normal was either PREPARED here or arrived with its magnitude",
          dut.normal_inputs_o, dut.normal_prepared_o + dut.supplied_mags_o);
    std::printf(
        "[stream] counters: normals=%u prepared=%u supplied=%u roots=%u/%u terms=%u/%u "
        "null=%u vtx=%u degen=%u degenTerms=%u rawSat=%u lo=%u hi=%u sat=%u seam=%u "
        "cfgRefused=%u epochRefused=%u tagMismatch=%u\n",
        dut.normal_inputs_o, dut.normal_prepared_o, dut.supplied_mags_o, dut.roots_issued_o,
        dut.roots_retired_o, dut.terms_accepted_o, dut.terms_retired_o, dut.terms_null_o,
        dut.vertices_lit_o, dut.degenerate_o, dut.degenerate_terms_o,
        dut.logical_raw_saturations_o, dut.ndl_clamp_lo_o, dut.ndl_clamp_hi_o, dut.rgb_sat_o,
        dut.seam_mismatch_o, dut.cfg_refused_o, dut.epoch_refusals_o, dut.tag_mismatch_o);
  }

  dut.final();
  zhao::exit_hard(zhao::report_and_exit("light_stream_directed"));
}
