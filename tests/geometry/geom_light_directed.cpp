// geom_light_directed.cpp — GEOM.LIGHT: the vertex-RGB composition around
// the SHARED light-term engine.
//
// ---------------------------------------------------------------------------
// WHAT HAS AN ORACLE HERE, AND WHAT DOES NOT. READ THIS BEFORE THE NUMBERS.
// ---------------------------------------------------------------------------
// `design/blocks.yml` declares GEOM.LIGHT's reference_model as
// `zref::render::shade_flat_tri_dir`, and `tools/budget/refmodel_liveness.py`
// RESOLVES it (84 of 95 declarations resolve; GEOM.LIGHT is not among the 11
// phantoms). So the light term is NOT a phantom and this bench is
// DIFFERENTIAL against it — specifically against its world-normal entry
// point `zref::render::shade_from_world_normal_unclamped`, the D-1
// one-level-down core that GEOM.LIGHT.md:134 exists to provide and that
// `zhao_terrain_shade` is the hardware of. The RTL under test instantiates
// that engine; this bench links the compiled law. Neither side restates it.
//
// THE COLOUR FOLD HAS NO ORACLE, AND THAT IS NOT AN OVERSIGHT.
// GEOM.LIGHT.md's "Scalar reference function" section says so in as many
// words — "What remains unwritten is the reference for the vertex-RGB half
// (multi-light accumulation, emission, ambient/spill, saturation order) —
// the composition laws are ruled above but have no executable oracle yet" —
// and SHADE-LIGHT-CONSOLIDATION-20260909 §5.1 records it as the V6 blocker
// keeping this block at maturity SPECIFIED.
//
// So the fold is checked two ways, and NEITHER of them is a differential:
//
//   * HAND-COMPUTED. Sections 2, 3, 4, 5 and 6 carry expectations a reader
//     can verify with a pencil — 0x10000, 0x8000, 0x4000 — because a fold
//     model written in this file and compared against RTL written from the
//     same prose is a duplicate checked against itself, which is EXACTLY the
//     failure that put twelve passing checks behind a second shade
//     implementation on 2026-09-03.
//   * SELF-CONSISTENCY, on the properties the ruling actually asserts: the
//     clamp is once per LIGHT (a turned-away second sun contributes zero and
//     does NOT subtract), detail lands INSIDE that clamp, emission==0 is
//     bit-exact, and the accumulator does not clip or wrap before the end.
//
// A pencil-checkable constant is weaker evidence than a differential and it
// is recorded as weaker. When the vertex-RGB reference is written, sections
// 2..6 should become differential against it and this comment should go.
//
// ---------------------------------------------------------------------------
// ONE HONEST CORRECTION TO THE CONTRACT'S OWN JUSTIFICATION
// ---------------------------------------------------------------------------
// GEOM.LIGHT.md argues the final saturate must happen once because
// "saturating per source would clip each contribution separately and CHANGE
// THE COLOUR OF AN OVERLAP". For the fold as ruled — a sum of non-negative
// terms — those two orders are arithmetically IDENTICAL: min(min(a,1)+b, 1)
// == min(a+b, 1) for a,b >= 0. The hue argument holds for a fold with a
// multiplicative or subtractive stage after the clip; this one has neither.
//
// So section 6 does NOT claim to demonstrate a hue difference it cannot
// produce. It pins the observable content of the ordering instead: with
// eight lights whose red total exceeds 1.0 and whose green total does not,
// red must land exactly on the rail and green must land exactly on its
// unclipped value — which an early clip or a narrow accumulator both break.
//
// ---------------------------------------------------------------------------
// COUNTERS
// ---------------------------------------------------------------------------
// All eleven are compared to a C-side tally with EXACT counts, and each is
// additionally seen to move as a DELTA ACROSS ITS OWN CASE, because a total
// that happens to match says nothing about which case moved it. No committed
// mutant is owed: every counter here is reachable with port stimulus (§13
// lists them one by one with the case that fires each). The one guard that
// IS structurally unreachable — the accumulator-width proof — is an
// elaboration `$fatal` rather than a runtime counter, and `--lint-only` does
// not execute it, so it is verified by this run and by nothing else.
//
// `light_terms_o` vs `engine_shaded_o` is the independent-path pair: offers
// accepted by the sequencer against walks completed inside the engine. They
// are not clocked by one enable, which is the whole reason the comparison
// can fire. It is what would catch the material combiner's defect — every
// colour correct while the machine did twice the work.
//
// POSITIVE CONTROL: --break-oracle corrupts ONE expected value in the
// differential tier by +1; the suite must then FAIL.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_geom_light.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_shade.hpp"

using zhao::check;

namespace {

constexpr int32_t kOne = 0x10000;  // 1.0 in Q16.16
constexpr uint32_t kLightsMax = 8;
constexpr uint32_t kEnvIdx = 0xF;

// The renderer's ONE key light, from the oracle header.
constexpr int32_t KLX = zref::terrain::kShadeLightX;
constexpr int32_t KLY = zref::terrain::kShadeLightY;
constexpr int32_t KLZ = zref::terrain::kShadeLightZ;

// ---------------------------------------------------------------------------
// The descriptor, mirroring the RTL's word map. Fields are named, never
// positional, so a word-order change is a compile error rather than a silent
// remap of colour onto emission.
// ---------------------------------------------------------------------------
struct Light {
  int32_t lx = 0, ly = 0, lz = 0;
  int32_t detail = 0;
  uint32_t cr = 0, cg = 0, cb = 0;  // colour gain, Q16.16 unsigned
  uint32_t er = 0, eg = 0, eb = 0;  // emission,     Q16.16 unsigned
};

struct Env {
  uint32_t ar = 0, ag = 0, ab = 0;  // ambient
  uint32_t sr = 0, sg = 0, sb = 0;  // spill
};

struct Rgb {
  uint32_t r = 0, g = 0, b = 0;
};

// C-side tallies for the exact-count assertions.
struct Tally {
  uint64_t vertices = 0;
  uint64_t degen = 0;
  uint64_t terms = 0;
  uint64_t clamp_lo = 0;
  uint64_t clamp_hi = 0;
  uint64_t rgb_sat = 0;
  uint64_t refused = 0;
  uint64_t nl_clamped = 0;
  uint64_t seam = 0;
} g_t;

// ---------------------------------------------------------------------------
// The fold, as ruled. NOT AN ORACLE — see the header. It calls the COMPILED
// law for the only part that has one and hand-implements only the prose the
// contract rules, so every check that uses it is a check against prose.
// ---------------------------------------------------------------------------
uint32_t rescale16_rhu(uint64_t p) { return static_cast<uint32_t>((p + 32768u) >> 16); }

int32_t law_raw(int32_t nx, int32_t ny, int32_t nz, int32_t lx, int32_t ly, int32_t lz) {
  return zref::render::shade_from_world_normal_unclamped(nx, ny, nz, lx, ly, lz, nullptr);
}

bool law_degenerate(int32_t nx, int32_t ny, int32_t nz) {
  return nx == 0 && ny == 0 && nz == 0;  // nmag2 == 0, the law's own arm
}

/** clamp01(raw + detail), once per light. Reports which rail it hit. */
uint32_t ndl_of(int32_t raw, int32_t detail, bool degen, bool* lo, bool* hi) {
  *lo = false;
  *hi = false;
  if (degen) return 0;  // no direction, no light term
  const int64_t sum = static_cast<int64_t>(raw) + static_cast<int64_t>(detail);
  if (sum < 0) *lo = true;
  if (sum > kOne) *hi = true;
  if (sum <= 0) return 0;
  if (sum >= kOne) return static_cast<uint32_t>(kOne);
  return static_cast<uint32_t>(sum);
}

struct FoldResult {
  Rgb rgb;
  bool degen = false;
  bool sat = false;
  uint32_t terms = 0;
  uint32_t clamp_lo = 0;
  uint32_t clamp_hi = 0;
};

FoldResult fold(int32_t nx, int32_t ny, int32_t nz, bool producer_degen,
                const std::vector<Light>& lights, uint32_t nlights, const Env& env) {
  FoldResult fr;
  uint64_t acc[3] = {0, 0, 0};
  // With no light evaluated the LAW is never asked, so the producer's flag is
  // the only verdict available — the RTL says the same in ST_IDLE.
  fr.degen = producer_degen;
  for (uint32_t i = 0; i < nlights; ++i) {
    const Light& L = lights[i];
    const bool degen = law_degenerate(nx, ny, nz);
    const int32_t raw = law_raw(nx, ny, nz, L.lx, L.ly, L.lz);
    bool lo = false, hi = false;
    const uint32_t ndl = ndl_of(raw, L.detail, degen, &lo, &hi);
    fr.clamp_lo += lo ? 1 : 0;
    fr.clamp_hi += hi ? 1 : 0;
    fr.degen = degen;  // the engine's own walk overrides the producer's flag
    fr.terms++;
    const uint32_t gains[6] = {L.cr, L.cg, L.cb, L.er, L.eg, L.eb};
    for (int k = 0; k < 6; ++k) acc[k % 3] += rescale16_rhu(static_cast<uint64_t>(gains[k]) * ndl);
  }
  const uint32_t amb[3] = {env.ar, env.ag, env.ab};
  const uint32_t spl[3] = {env.sr, env.sg, env.sb};
  uint32_t out[3];
  for (int c = 0; c < 3; ++c) {
    const uint64_t tot = acc[c] + amb[c] + spl[c];
    if (tot > static_cast<uint64_t>(kOne)) fr.sat = true;
    out[c] = (tot > static_cast<uint64_t>(kOne)) ? static_cast<uint32_t>(kOne)
                                                 : static_cast<uint32_t>(tot);
  }
  fr.rgb = {out[0], out[1], out[2]};
  return fr;
}

// ---------------------------------------------------------------------------
// DUT driving
// ---------------------------------------------------------------------------
// Every tick goes through here so INITIATION INTERVAL can be measured rather
// than inferred. The owner plan of 2026-09-18 §7.2 is specific about which
// number decides the schedule: "The current service's ACTUAL INITIATION
// INTERVAL, not just its latency, decides this." A block whose latency and II
// happen to be equal — this one holds a single packet and overlaps nothing —
// still owes the measurement, because the moment anything overlaps they part
// company and a quoted latency silently becomes an optimistic II.
uint64_t g_cycles = 0;
void tk(Vzhao_geom_light& d) {
  zhao::tick(d);
  ++g_cycles;
}

void reset_dut(Vzhao_geom_light& dut) {
  dut.rst_n = 0;
  dut.cfg_we_i = 0;
  dut.cfg_addr_i = 0;
  dut.cfg_data_i = 0;
  dut.v_valid_i = 0;
  dut.r_ready_i = 0;
  dut.n_x_i = 0;
  dut.n_y_i = 0;
  dut.n_z_i = 0;
  dut.n_degenerate_i = 0;
  dut.nlights_i = 0;
  dut.src_id_i = 0;
  dut.eval();
  for (int i = 0; i < 2; ++i) tk(dut);
  dut.rst_n = 1;
  dut.eval();
  tk(dut);
}

void cfg_write(Vzhao_geom_light& dut, uint32_t idx, uint32_t word, uint32_t data) {
  dut.cfg_we_i = 1;
  dut.cfg_addr_i = static_cast<uint8_t>(((idx & 0xF) << 4) | (word & 0xF));
  dut.cfg_data_i = data;
  tk(dut);
  dut.cfg_we_i = 0;
  dut.cfg_addr_i = 0;
  dut.cfg_data_i = 0;
}

void load_light(Vzhao_geom_light& dut, uint32_t idx, const Light& L) {
  cfg_write(dut, idx, 0, static_cast<uint32_t>(L.lx));
  cfg_write(dut, idx, 1, static_cast<uint32_t>(L.ly));
  cfg_write(dut, idx, 2, static_cast<uint32_t>(L.lz));
  cfg_write(dut, idx, 3, static_cast<uint32_t>(L.detail));
  cfg_write(dut, idx, 4, L.cr);
  cfg_write(dut, idx, 5, L.cg);
  cfg_write(dut, idx, 6, L.cb);
  cfg_write(dut, idx, 7, L.er);
  cfg_write(dut, idx, 8, L.eg);
  cfg_write(dut, idx, 9, L.eb);
}

void load_env(Vzhao_geom_light& dut, const Env& e) {
  cfg_write(dut, kEnvIdx, 0, e.ar);
  cfg_write(dut, kEnvIdx, 1, e.ag);
  cfg_write(dut, kEnvIdx, 2, e.ab);
  cfg_write(dut, kEnvIdx, 3, e.sr);
  cfg_write(dut, kEnvIdx, 4, e.sg);
  cfg_write(dut, kEnvIdx, 5, e.sb);
}

struct DriveResult {
  Rgb rgb;
  bool degen = false;
  uint16_t src = 0;
  int latency = 0;
};

DriveResult drive(Vzhao_geom_light& dut, int32_t nx, int32_t ny, int32_t nz, bool producer_degen,
                  uint32_t nlights, uint16_t src, int stall = 0) {
  int guard = 0;
  while (!dut.v_ready_o) {
    tk(dut);
    if (++guard > 4000) {
      check(false, "v_ready_o never rose", 1, 0);
      zhao::exit_hard(zhao::report_and_exit("geom_light_directed"));
    }
  }
  dut.v_valid_i = 1;
  dut.n_x_i = static_cast<uint32_t>(nx);
  dut.n_y_i = static_cast<uint32_t>(ny);
  dut.n_z_i = static_cast<uint32_t>(nz);
  dut.n_degenerate_i = producer_degen ? 1 : 0;
  dut.nlights_i = static_cast<uint8_t>(nlights & 0xF);
  dut.src_id_i = src;
  dut.r_ready_i = 0;
  tk(dut);  // accept edge
  dut.v_valid_i = 0;

  int lat = 0;
  while (!dut.r_valid_o) {
    tk(dut);
    if (++lat > 400000) {
      check(false, "r_valid_o never rose", 1, 0);
      zhao::exit_hard(zhao::report_and_exit("geom_light_directed"));
    }
  }

  DriveResult r;
  r.rgb = {dut.rgb_r_o, dut.rgb_g_o, dut.rgb_b_o};
  r.degen = dut.degenerate_vtx_o != 0;
  r.src = static_cast<uint16_t>(dut.src_id_o);
  r.latency = lat;

  // Backpressure: the packet HOLDS, byte for byte, while the consumer stalls.
  for (int i = 0; i < stall; ++i) {
    tk(dut);
    check(dut.r_valid_o == 1, "r_valid_o held under stall", 1, dut.r_valid_o);
    check(dut.rgb_r_o == r.rgb.r, "rgb_r_o stable under stall", r.rgb.r, dut.rgb_r_o);
    check(dut.rgb_g_o == r.rgb.g, "rgb_g_o stable under stall", r.rgb.g, dut.rgb_g_o);
    check(dut.rgb_b_o == r.rgb.b, "rgb_b_o stable under stall", r.rgb.b, dut.rgb_b_o);
    check(static_cast<uint16_t>(dut.src_id_o) == r.src, "src_id_o stable under stall", r.src,
          dut.src_id_o);
  }
  dut.r_ready_i = 1;
  tk(dut);
  dut.r_ready_i = 0;
  return r;
}

/** Drive one vertex, tally what the ruling says it does, compare the packet. */
DriveResult check_vertex(Vzhao_geom_light& dut, int32_t nx, int32_t ny, int32_t nz,
                         bool producer_degen, const std::vector<Light>& lights,
                         uint32_t nlights_req, const Env& env, const char* what,
                         uint16_t src = 0x1234, int stall = 0, int32_t inject = 0) {
  const uint32_t nl_eff = (nlights_req > kLightsMax) ? kLightsMax : nlights_req;
  if (nlights_req > kLightsMax) g_t.nl_clamped++;
  const FoldResult f = fold(nx, ny, nz, producer_degen, lights, nl_eff, env);
  const DriveResult r = drive(dut, nx, ny, nz, producer_degen, nlights_req, src, stall);

  g_t.vertices++;
  g_t.terms += f.terms;
  g_t.clamp_lo += f.clamp_lo;
  g_t.clamp_hi += f.clamp_hi;
  if (f.degen) g_t.degen++;
  if (f.sat) g_t.rgb_sat++;
  // The engine's seam detector: producer flag vs the law's own nmag2==0 walk,
  // once per engine turn.
  if (producer_degen != law_degenerate(nx, ny, nz)) g_t.seam += f.terms;

  const uint32_t er = f.rgb.r + static_cast<uint32_t>(inject);
  check(r.rgb.r == er, what, er, r.rgb.r);
  check(r.rgb.g == f.rgb.g, "green channel", f.rgb.g, r.rgb.g);
  check(r.rgb.b == f.rgb.b, "blue channel", f.rgb.b, r.rgb.b);
  check(r.degen == f.degen, "degenerate_vtx_o", f.degen ? 1 : 0, r.degen ? 1 : 0);
  check(r.src == src, "src_id rides the vertex", src, r.src);
  return r;
}

// xorshift, deterministic.
uint64_t g_rng = 0x9E3779B97F4A7C15ULL;
uint64_t rnd() {
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 7;
  g_rng ^= g_rng << 17;
  return g_rng;
}
int32_t rnd_s32() { return static_cast<int32_t>(rnd()); }
int32_t rnd_biased() {
  switch (rnd() & 3) {
    case 0:
      return static_cast<int32_t>(rnd() & 7) - 3;  // the near-degenerate band
    case 1:
      return rnd_s32() >> 16;
    case 2:
      return (rnd() & 1) ? INT32_MAX - static_cast<int32_t>(rnd() & 3)
                         : INT32_MIN + static_cast<int32_t>(rnd() & 3);
    default:
      return rnd_s32();
  }
}

}  // namespace

int main(int argc, char** argv) {
  bool break_oracle = false;
  for (int i = 1; i < argc; ++i)
    if (!std::strcmp(argv[i], "--break-oracle")) break_oracle = true;

  Verilated::commandArgs(argc, argv);

  Vzhao_geom_light* dutp = new Vzhao_geom_light;  // heap-allocated, harness rule
  Vzhao_geom_light& dut = *dutp;
  reset_dut(dut);

  std::vector<Light> L(kLightsMax);
  Env env;

  // ---- 1: COLD FILL — the shared engine gates this block's ready ---------
  // zhao_terrain_shade fills its quarter-square table for 512 cycles after
  // reset. A shell that offered a vertex during the fill would hang on a
  // ready that is not coming.
  {
    check(dut.table_ready_o == 0, "table not ready at reset", 0, dut.table_ready_o);
    check(dut.v_ready_o == 0, "v_ready_o low during the engine's cold fill", 0, dut.v_ready_o);
    int fill = 0;
    while (!dut.table_ready_o && fill < 700) {
      tk(dut);
      ++fill;
    }
    check(dut.table_ready_o == 1, "engine table fill completes", 1, dut.table_ready_o);
    check(fill <= 515, "fill takes the engine's declared 512 cycles", 512,
          static_cast<uint32_t>(fill));
    check(dut.v_ready_o == 1, "v_ready_o rises once the engine can start", 1, dut.v_ready_o);
    check(dut.idle_o == 1, "idle after fill", 1, dut.idle_o);
  }

  // ---- 2: HAND-COMPUTED, ONE LIGHT ---------------------------------------
  // A +Y face under an overhead sun is FULLY lit: raw == 0x10000 exactly.
  // Gains 1.0 / 0.5 / 0.25 must produce 0x10000 / 0x8000 / 0x4000 — numbers
  // a reader checks with a pencil, not with this file's own fold model.
  {
    const uint64_t c0 = dut.cfg_refused_o;
    L[0] = Light{};
    L[0].lx = 0;
    L[0].ly = kOne;
    L[0].lz = 0;
    L[0].cr = 0x10000;
    L[0].cg = 0x8000;
    L[0].cb = 0x4000;
    load_light(dut, 0, L[0]);
    load_env(dut, env);
    check(dut.cfg_refused_o == c0, "legal descriptor writes are not refused", c0,
          dut.cfg_refused_o);

    const int32_t raw = law_raw(0, kOne, 0, 0, kOne, 0);
    check(raw == kOne, "the compiled law: +Y normal, overhead sun is exactly 1.0",
          static_cast<uint32_t>(kOne), static_cast<uint32_t>(raw));

    const DriveResult r = check_vertex(dut, 0, kOne, 0, false, L, 1, env,
                                       "one light, unit gain: red is exactly 0x10000", 0x0101, 3);
    check(r.rgb.r == 0x10000u, "hand-computed red  == 0x10000", 0x10000u, r.rgb.r);
    check(r.rgb.g == 0x08000u, "hand-computed green == 0x8000", 0x08000u, r.rgb.g);
    check(r.rgb.b == 0x04000u, "hand-computed blue  == 0x4000", 0x04000u, r.rgb.b);
  }

  // ---- 3: THE D-1 RULING — THE CLAMP IS ONCE PER LIGHT -------------------
  // This is the section the ruling exists for. Light 0 faces the surface;
  // light 1 is a second INDEPENDENT sun behind it, whose dot is negative. Its
  // contribution must be ZERO and must NOT subtract what light 0 gave. An
  // implementation that clamped once per vertex instead would return light
  // 0's value MINUS light 1's magnitude — here, black.
  {
    const uint64_t lo0 = dut.ndl_clamp_lo_o;
    L[0] = Light{};
    L[0].ly = kOne;
    L[0].cr = 0x8000;  // 0.5
    L[0].cg = 0x8000;
    L[0].cb = 0x8000;
    L[1] = Light{};
    L[1].ly = -kOne;  // the same sun from behind: raw is exactly -1.0
    L[1].cr = 0x10000;
    L[1].cg = 0x10000;
    L[1].cb = 0x10000;
    load_light(dut, 0, L[0]);
    load_light(dut, 1, L[1]);

    check(law_raw(0, kOne, 0, 0, -kOne, 0) == -kOne,
          "the compiled law: the turned-away sun really is -1.0", static_cast<uint32_t>(-kOne),
          static_cast<uint32_t>(law_raw(0, kOne, 0, 0, -kOne, 0)));

    const DriveResult r =
        check_vertex(dut, 0, kOne, 0, false, L, 2, env,
                     "two lights, one turned away: the negative one contributes 0", 0x0202);
    check(r.rgb.r == 0x8000u,
          "D-1: a second sun's negative dot does NOT subtract the first sun's light", 0x8000u,
          r.rgb.r);
    check(dut.ndl_clamp_lo_o == lo0 + 1, "ndl_clamp_lo_o moved by exactly 1 across this case",
          lo0 + 1, dut.ndl_clamp_lo_o);
  }

  // ---- 4: DETAIL IS INSIDE THIS LIGHT'S CLAMP ----------------------------
  // "A detail normal may brighten a face turned slightly from THAT SAME sun,
  // so base and detail combine BEFORE clamping." A face whose raw is a small
  // negative plus a larger positive detail must come out LIT. Adding detail
  // to an already-clamped zero could not do that.
  {
    const uint64_t lo0 = dut.ndl_clamp_lo_o;
    const uint64_t hi0 = dut.ndl_clamp_hi_o;

    // (a) small negative raw rescued by detail
    L[0] = Light{};
    L[0].ly = -kOne / 8;     // raw = -0.125
    L[0].detail = kOne / 4;  // detail = +0.25 -> ndl = 0.125
    L[0].cr = 0x10000;
    L[0].cg = 0;
    L[0].cb = 0;
    load_light(dut, 0, L[0]);
    const DriveResult ra =
        check_vertex(dut, 0, kOne, 0, false, L, 1, env,
                     "detail inside the clamp rescues a slightly turned face", 0x0301);
    check(ra.rgb.r == 0x2000u, "hand-computed: (-0.125 + 0.25) * 1.0 == 0x2000", 0x2000u, ra.rgb.r);
    check(dut.ndl_clamp_lo_o == lo0, "no low clamp on the rescued face", lo0, dut.ndl_clamp_lo_o);

    // (b) detail pushing past 1.0 — the high rail
    L[0].ly = kOne;
    L[0].detail = kOne;  // 1.0 + 1.0 -> clamped to 1.0
    load_light(dut, 0, L[0]);
    const DriveResult rb = check_vertex(dut, 0, kOne, 0, false, L, 1, env,
                                        "detail past full scale clamps at 1.0", 0x0302);
    check(rb.rgb.r == 0x10000u, "hand-computed: clamp01(1.0 + 1.0) == 0x10000", 0x10000u, rb.rgb.r);
    check(dut.ndl_clamp_hi_o == hi0 + 1, "ndl_clamp_hi_o moved by exactly 1 across this case",
          hi0 + 1, dut.ndl_clamp_hi_o);
    L[0].detail = 0;
  }

  // ---- 5: THE EMISSION CUT SEAM ------------------------------------------
  // The provisional additive term. Two properties, both of which are what
  // make it safe to adopt provisionally:
  //   (a) emission == 0 is a BIT-EXACT no-op;
  //   (b) emission uses the SAME per-light lambert response as the gain — a
  //       flat additive lift would raise lit and unlit faces equally and
  //       FLATTEN FORM, which is the failure the creature rig was rewritten
  //       to cure. A face turned away must receive NO emission.
  {
    L[0] = Light{};
    L[0].ly = kOne;
    L[0].cr = 0x4000;  // 0.25
    L[0].cg = 0;
    L[0].cb = 0;
    load_light(dut, 0, L[0]);
    const DriveResult off =
        check_vertex(dut, 0, kOne, 0, false, L, 1, env, "emission gate shut", 0x0401);

    L[0].er = 0x8000;  // +0.5 additive
    load_light(dut, 0, L[0]);
    const DriveResult on =
        check_vertex(dut, 0, kOne, 0, false, L, 1, env, "emission gate open, lit face", 0x0402);
    check(off.rgb.r == 0x4000u, "emission==0 is the multiplicative-only value", 0x4000u, off.rgb.r);
    check(on.rgb.r == 0xC000u, "emission ADDS energy: 0.25 + 0.5 == 0xC000", 0xC000u, on.rgb.r);

    // the same emitter on a face turned AWAY: ndl == 0, so nothing is added.
    L[0].ly = -kOne;
    load_light(dut, 0, L[0]);
    const DriveResult away = check_vertex(dut, 0, kOne, 0, false, L, 1, env,
                                          "emission on a turned-away face adds nothing", 0x0403);
    check(away.rgb.r == 0u, "emission follows the lambert response and does not flatten form", 0u,
          away.rgb.r);
  }

  // ---- 6: ONE SATURATE, AND THE ACCUMULATOR THAT MAKES IT POSSIBLE -------
  // Eight lights, all fully lit. Red totals 8 * 0.2 == 1.6 and must land
  // EXACTLY on the rail; green totals 8 * 0.05 == 0.4 and must land EXACTLY
  // on its unclipped value. An accumulator that clipped early or wrapped
  // breaks the green, and only the green.
  {
    const uint64_t sat0 = dut.rgb_sat_o;
    const uint64_t terms0 = dut.light_terms_o;
    const uint32_t gr = 0x10000 / 5;   // 0.2
    const uint32_t gg = 0x10000 / 20;  // 0.05
    for (uint32_t i = 0; i < kLightsMax; ++i) {
      L[i] = Light{};
      L[i].ly = kOne;
      L[i].cr = gr;
      L[i].cg = gg;
      L[i].cb = 0;
      load_light(dut, i, L[i]);
    }
    const DriveResult r = check_vertex(dut, 0, kOne, 0, false, L, kLightsMax, env,
                                       "eight lights: red saturates, green does not", 0x0501);
    check(r.rgb.r == 0x10000u, "red lands exactly on the 1.0 rail", 0x10000u, r.rgb.r);
    check(r.rgb.g == 8u * gg, "green lands exactly on its unclipped sum (no early clip, no wrap)",
          8u * gg, r.rgb.g);
    check(dut.rgb_sat_o == sat0 + 1, "rgb_sat_o moved by exactly 1 across this case", sat0 + 1,
          dut.rgb_sat_o);
    check(dut.light_terms_o == terms0 + kLightsMax,
          "light_terms_o moved by exactly LIGHTS_MAX: one engine, eight turns", terms0 + kLightsMax,
          dut.light_terms_o);
  }

  // ---- 7: AMBIENT AND SPILL ARE COLOUR, NOT DIRECTIONAL ------------------
  // They need no dot product and they apply to a face the sun cannot reach.
  {
    env.ar = 0x2000;
    env.ag = 0x1000;
    env.ab = 0x0800;
    env.sr = 0x1000;
    env.sg = 0;
    env.sb = 0;
    load_env(dut, env);
    L[0] = Light{};
    L[0].ly = -kOne;  // turned fully away
    L[0].cr = 0x10000;
    L[0].cg = 0x10000;
    L[0].cb = 0x10000;
    load_light(dut, 0, L[0]);
    const DriveResult r = check_vertex(dut, 0, kOne, 0, false, L, 1, env,
                                       "an unlit face still receives ambient and spill", 0x0601);
    check(r.rgb.r == 0x3000u, "hand-computed: ambient 0x2000 + spill 0x1000", 0x3000u, r.rgb.r);
    check(r.rgb.g == 0x1000u, "hand-computed green ambient", 0x1000u, r.rgb.g);
  }

  // ---- 8: nlights == 0 — PURE ENVIRONMENT, NO ENGINE TURN ----------------
  {
    const uint64_t terms0 = dut.light_terms_o;
    const DriveResult r = check_vertex(dut, 0, kOne, 0, false, L, 0, env,
                                       "nlights==0 emits ambient+spill only", 0x0701);
    check(r.rgb.r == 0x3000u, "environment-only red", 0x3000u, r.rgb.r);
    check(dut.light_terms_o == terms0,
          "light_terms_o did NOT move: a vertex with no lights costs no engine turn", terms0,
          dut.light_terms_o);
  }

  // ---- 9: THE DEGENERATE VERTEX ------------------------------------------
  // GEOM.LIGHT.md: "A zero-length normal is degenerate: shade 0, matching the
  // ratified law's nmag2 == 0 guard. Counted." So it receives AMBIENT ONLY —
  // TERRAIN.SHADE.md's "a dark face, never a bright one". It is NOT black:
  // GEOM.SKIN.NORM.md says "light it black" and the two contracts disagree,
  // so the reading is a parameter (DEGEN_BLACK, default 0) and this is the
  // default's behaviour.
  {
    const uint64_t d0 = dut.degenerate_o;
    const uint64_t s0 = dut.seam_mismatch_o;
    const uint64_t ed0 = dut.engine_degen_o;
    L[0] = Light{};
    L[0].ly = kOne;
    L[0].detail = kOne;  // detail must NOT resurrect a directionless vertex
    L[0].cr = 0x10000;
    L[0].cg = 0x10000;
    L[0].cb = 0x10000;
    load_light(dut, 0, L[0]);
    const DriveResult r = check_vertex(dut, 0, 0, 0, true, L, 1, env,
                                       "degenerate normal: shade 0, ambient only", 0x0801);
    check(r.rgb.r == 0x3000u, "degenerate vertex gets environment and no light term", 0x3000u,
          r.rgb.r);
    check(r.degen == true, "degenerate_vtx_o rides the packet", 1, r.degen ? 1 : 0);
    check(dut.degenerate_o == d0 + 1, "degenerate_o moved by exactly 1 across this case", d0 + 1,
          dut.degenerate_o);
    check(dut.seam_mismatch_o == s0,
          "seam_mismatch_o quiet when the producer's flag agrees with the law", s0,
          dut.seam_mismatch_o);
    check(dut.engine_degen_o == ed0 + 1,
          "engine_degen_o moved by exactly 1: the LAW judged this vertex, not the shell", ed0 + 1,
          dut.engine_degen_o);
  }

  // ---- 9b: THE ENGINE'S OWN INT32 CLAMP ----------------------------------
  // `engine_base_sat_o` is forwarded, and a forwarded counter asserted zero
  // is still a counter nobody has seen move. The law's domain is ANY int32
  // light, not just unit suns, and a rail sun on a tiny normal drives the
  // quotient past INT32 — where `div_rhu_s128`'s own clamp engages. The
  // composed vertex is unremarkable (ndl saturates at 1.0 either way), which
  // is exactly why the counter has to be read rather than inferred.
  {
    const uint64_t bs0 = dut.engine_base_sat_o;
    L[0] = Light{};
    L[0].lx = INT32_MAX;
    L[0].ly = INT32_MAX;
    L[0].lz = 0;
    L[0].cr = 0x10000;
    L[0].cg = 0;
    L[0].cb = 0;
    load_light(dut, 0, L[0]);
    Env zero;
    load_env(dut, zero);
    const DriveResult r = check_vertex(dut, 1, 1, 0, false, L, 1, zero,
                                       "rail sun: the engine's INT32 clamp engages", 0x08B1);
    check(r.rgb.r == 0x10000u, "a saturated base still clamps to exactly 1.0", 0x10000u, r.rgb.r);
    check(dut.engine_base_sat_o == bs0 + 1, "engine_base_sat_o moved by exactly 1 across this case",
          bs0 + 1, dut.engine_base_sat_o);
    load_env(dut, env);
  }

  // ---- 10: THE SEAM DETECTOR FIRES ---------------------------------------
  // The engine's degen_mismatch_o, forwarded. Its two operands arrive by
  // INDEPENDENT paths — the producer's flag on the port, the nmag2==0
  // verdict from the engine's own walk — which is the only reason it can
  // fire at all. A detector reading zero is a claim; here it is fired
  // deliberately, in both directions.
  {
    const uint64_t s0 = dut.seam_mismatch_o;
    // Reload light 0 explicitly. The previous section left `detail` set in the
    // DUT's descriptor bank, and a C-side struct field reset without a
    // matching `load_light` is a model and a machine that have quietly parted
    // company — it cost one off-by-one on ndl_clamp_hi_o before the exact
    // counter tally caught it. A counter comparison found what three
    // value checks could not.
    L[0] = Light{};
    L[0].ly = kOne;
    L[0].cr = 0x10000;
    L[0].cg = 0x10000;
    L[0].cb = 0x10000;
    load_light(dut, 0, L[0]);
    check_vertex(dut, 0, kOne, 0, /*producer_degen=*/true, L, 1, env,
                 "producer says degenerate, normal is real: the law wins", 0x0901);
    check(dut.seam_mismatch_o == s0 + 1, "seam_mismatch_o fired on flag-high/normal-real", s0 + 1,
          dut.seam_mismatch_o);
    check_vertex(dut, 0, 0, 0, /*producer_degen=*/false, L, 1, env,
                 "producer says fine, normal is zero: the law wins", 0x0902);
    check(dut.seam_mismatch_o == s0 + 2, "seam_mismatch_o fired on flag-low/normal-zero", s0 + 2,
          dut.seam_mismatch_o);
  }

  // ---- 11: nlights ABOVE THE DESCRIPTOR SET IS CLAMPED AND COUNTED -------
  // Never silently aliased onto a slot that does not exist.
  {
    const uint64_t c0 = dut.nlights_clamped_o;
    const uint64_t t0 = dut.light_terms_o;
    for (uint32_t i = 0; i < kLightsMax; ++i) {
      L[i] = Light{};
      L[i].ly = kOne;
      L[i].cr = 0x2000;
      load_light(dut, i, L[i]);
    }
    check_vertex(dut, 0, kOne, 0, false, L, 15, env, "nlights=15 clamps to LIGHTS_MAX", 0x0A01);
    check(dut.nlights_clamped_o == c0 + 1, "nlights_clamped_o moved by exactly 1", c0 + 1,
          dut.nlights_clamped_o);
    check(dut.light_terms_o == t0 + kLightsMax, "the clamped vertex ran exactly LIGHTS_MAX turns",
          t0 + kLightsMax, dut.light_terms_o);
  }

  // ---- 12: THE DESCRIPTOR MAP REFUSES, IT DOES NOT ALIAS -----------------
  // A masked index would fold an illegal write onto light 0 in silence. The
  // check that separates the two is not the counter — it is lighting a
  // vertex afterwards and seeing light 0 unchanged.
  {
    const uint64_t c0 = dut.cfg_refused_o;
    L[0] = Light{};
    L[0].ly = kOne;
    L[0].cr = 0x4000;
    load_light(dut, 0, L[0]);
    const DriveResult before = check_vertex(dut, 0, kOne, 0, false, L, 1, env,
                                            "light 0 before the illegal writes", 0x0B01);

    cfg_write(dut, kLightsMax, 0, 0xDEADBEEF);      // index past the set
    cfg_write(dut, kLightsMax + 3, 4, 0xDEADBEEF);  // another
    cfg_write(dut, 0, 10, 0xDEADBEEF);              // word past the descriptor
    cfg_write(dut, 0, 15, 0xDEADBEEF);              // last word of the field
    cfg_write(dut, kEnvIdx, 6, 0xDEADBEEF);         // word past the environment
    g_t.refused += 5;

    check(dut.cfg_refused_o == c0 + 5, "cfg_refused_o moved by exactly 5 across this case", c0 + 5,
          dut.cfg_refused_o);
    const DriveResult after = check_vertex(dut, 0, kOne, 0, false, L, 1, env,
                                           "light 0 is untouched by the refused writes", 0x0B02);
    check(after.rgb.r == before.rgb.r, "a refused write did not alias onto light 0", before.rgb.r,
          after.rgb.r);
  }

  // ---- 13: BACKPRESSURE IS TRANSPARENT -----------------------------------
  // A stalled consumer must not change a value. Same vertex, three stall
  // lengths, byte-identical results.
  {
    L[0] = Light{};
    L[0].lx = KLX;
    L[0].ly = KLY;
    L[0].lz = KLZ;
    L[0].cr = 0xC000;
    L[0].cg = 0x9000;
    L[0].cb = 0x3000;
    load_light(dut, 0, L[0]);
    const DriveResult a =
        check_vertex(dut, 3000, 40000, -9000, false, L, 1, env, "key light, no stall", 0x0C01, 0);
    const DriveResult b = check_vertex(dut, 3000, 40000, -9000, false, L, 1, env,
                                       "key light, 5-cycle stall", 0x0C02, 5);
    const DriveResult c = check_vertex(dut, 3000, 40000, -9000, false, L, 1, env,
                                       "key light, 40-cycle stall", 0x0C03, 40);
    check(a.rgb.r == b.rgb.r && b.rgb.r == c.rgb.r, "stalling does not move the answer (r)",
          a.rgb.r, c.rgb.r);
    check(a.rgb.g == b.rgb.g && b.rgb.g == c.rgb.g, "stalling does not move the answer (g)",
          a.rgb.g, c.rgb.g);
    check(a.latency == b.latency, "latency is stall-independent up to the handshake",
          static_cast<uint32_t>(a.latency), static_cast<uint32_t>(b.latency));
    std::printf("[light] one-light accept->valid: %d cycles\n", a.latency);
  }

  // ---- 13b: THE INITIATION INTERVAL, MEASURED ---------------------------
  // The owner plan of 2026-09-18 §7.2: "The current service's ACTUAL
  // INITIATION INTERVAL, not just its latency, decides this." So this
  // section measures the sustained rate with a consumer that never stalls,
  // and prints the frame arithmetic rather than leaving it to be re-derived
  // from a latency number that only coincidentally equals it today.
  //
  // It asserts a CEILING, not an equality. A future overlap of the
  // descriptor fetch or the MAC under the engine's walk should make this
  // number FALL, and a test that pinned it exactly would go red on an
  // improvement — which is how a throughput gate ends up deleted.
  {
    const uint64_t c0 = g_cycles;
    const uint64_t t0 = dut.light_terms_o;
    constexpr int kBurst = 16;
    for (int i = 0; i < kBurst; ++i)
      check_vertex(dut, 3000, 40000, -9000, false, L, 1, env, "II burst vertex",
                   static_cast<uint16_t>(0x0D00 + i), 0);
    const uint64_t spent = g_cycles - c0;
    const uint64_t terms = dut.light_terms_o - t0;
    const double ii_light = static_cast<double>(spent) / static_cast<double>(terms);

    check(terms == kBurst, "the burst ran exactly one engine turn per vertex",
          static_cast<uint64_t>(kBurst), terms);
    // 200 is a ceiling with headroom over the ~165 designed, not a target.
    check(ii_light < 200.0, "sustained II per light term stays under the declared ceiling", 200,
          static_cast<uint64_t>(ii_light));

    // The plan's stress profile, carried through THIS measurement. Nothing
    // here narrows the workload to make the number look better; §7.2 forbids
    // exactly that ("rather than quietly lowering the admitted workload").
    const double evals = 120000.0 * 4.0;  // §7.2's stated stress profile
    const double frame = 1666666.0;       // 100 MHz / 60 Hz
    const double need = evals * ii_light;
    std::printf(
        "[light] MEASURED II = %.1f clk/light-term (%llu clk / %llu terms). "
        "Plan §7.2 profile 120k vtx x 4 lights = 480k evaluations -> %.0f clk "
        "against %.0f/frame = %.1fx OVER. NOT creature rate; see the RTL "
        "header's reported cost and lever order.\n",
        ii_light, static_cast<unsigned long long>(spent), static_cast<unsigned long long>(terms),
        need, frame, need / frame);
  }

  // ---- 14: THE DIFFERENTIAL TIER ----------------------------------------
  // With a single light at unit red gain, zero detail and zero environment,
  // rgb_r is EXACTLY clamp01(shade_from_world_normal_unclamped(n, L)) —
  // because (0x10000 * ndl + 32768) >> 16 == ndl. So this section compares
  // the composed block against the COMPILED ratified law over the legal
  // normal and light space, which is the test GEOM.LIGHT.md calls "the one
  // that matters most".
  int cov_lit = 0, cov_dark = 0, cov_full = 0;
  {
    Env zero;
    load_env(dut, zero);
    for (int i = 0; i < 400; ++i) {
      Light d;
      d.lx = KLX;
      d.ly = KLY;
      d.lz = KLZ;
      const uint64_t mode = rnd() & 3;
      if (mode == 1) {
        d.lx = rnd_s32();
        d.ly = rnd_s32();
        d.lz = rnd_s32();
      } else if (mode == 2) {
        d.lx = static_cast<int32_t>(rnd() % 131073) - 65536;
        d.ly = static_cast<int32_t>(rnd() % 131073) - 65536;
        d.lz = static_cast<int32_t>(rnd() % 131073) - 65536;
      }
      d.cr = 0x10000;
      d.cg = 0;
      d.cb = 0;
      L[0] = d;
      load_light(dut, 0, d);

      const int32_t nx = rnd_biased();
      const int32_t ny = rnd_biased();
      const int32_t nz = rnd_biased();
      const bool degen = law_degenerate(nx, ny, nz);
      const int32_t raw = law_raw(nx, ny, nz, d.lx, d.ly, d.lz);
      const uint32_t want = degen ? 0u
                                  : (raw <= 0 ? 0u
                                              : (raw >= kOne ? static_cast<uint32_t>(kOne)
                                                             : static_cast<uint32_t>(raw)));
      if (want == 0) cov_dark++;
      if (want > 0 && want < static_cast<uint32_t>(kOne)) cov_lit++;
      if (want == static_cast<uint32_t>(kOne)) cov_full++;

      const int32_t inject = (break_oracle && i == 7) ? 1 : 0;
      const DriveResult r =
          check_vertex(dut, nx, ny, nz, degen, L, 1, zero,
                       "DIFFERENTIAL: rgb_r == clamp01(compiled shade_from_world_normal)",
                       static_cast<uint16_t>(rnd() & 0xFFFF), (i % 31 == 0) ? 2 : 0, inject);
      check(r.rgb.r == want + static_cast<uint32_t>(inject), "differential value",
            want + static_cast<uint32_t>(inject), r.rgb.r);
    }
    load_env(dut, env);
  }

  // ---- 15: RANDOM MULTI-LIGHT FOLD, SELF-CONSISTENCY ---------------------
  // Not a differential — the fold model is this file's own reading of the
  // ruled prose. Its value is coverage of the sequencer under varying
  // nlights, gains and stalls, and the counter tallies it keeps honest.
  {
    for (int i = 0; i < 90; ++i) {
      const uint32_t n = 1 + static_cast<uint32_t>(rnd() % kLightsMax);
      for (uint32_t k = 0; k < n; ++k) {
        Light d;
        d.lx = static_cast<int32_t>(rnd() % 131073) - 65536;
        d.ly = static_cast<int32_t>(rnd() % 131073) - 65536;
        d.lz = static_cast<int32_t>(rnd() % 131073) - 65536;
        d.detail = (rnd() & 7) ? 0 : (static_cast<int32_t>(rnd() % 131073) - 65536);
        d.cr = static_cast<uint32_t>(rnd() % 0x20000);
        d.cg = static_cast<uint32_t>(rnd() % 0x20000);
        d.cb = static_cast<uint32_t>(rnd() % 0x20000);
        d.er = (rnd() & 3) ? 0 : static_cast<uint32_t>(rnd() % 0x10000);
        d.eg = (rnd() & 3) ? 0 : static_cast<uint32_t>(rnd() % 0x10000);
        d.eb = (rnd() & 3) ? 0 : static_cast<uint32_t>(rnd() % 0x10000);
        L[k] = d;
        load_light(dut, k, d);
      }
      // One vertex in eight is EXACTLY degenerate. `rnd_biased()` almost never
      // lands all three lanes on zero, so leaving it to chance samples the
      // no-direction path about twice in five hundred — a coverage number that
      // reads like coverage and is not one.
      const bool force_degen = (rnd() % 8) == 0;
      const int32_t nx = force_degen ? 0 : rnd_biased();
      const int32_t ny = force_degen ? 0 : rnd_biased();
      const int32_t nz = force_degen ? 0 : rnd_biased();
      check_vertex(dut, nx, ny, nz, law_degenerate(nx, ny, nz), L, n, env,
                   "random multi-light fold", static_cast<uint16_t>(rnd() & 0xFFFF),
                   (i % 17 == 0) ? 3 : 0);
    }
  }

  // ---- 16: EVERY COUNTER, EXACT ------------------------------------------
  // Totals, after every per-case delta above. A total that matches proves
  // nothing about which case moved it, which is why both are here.
  {
    check(dut.vertices_lit_o == g_t.vertices, "vertices_lit_o exact", g_t.vertices,
          dut.vertices_lit_o);
    check(dut.degenerate_o == g_t.degen, "degenerate_o exact", g_t.degen, dut.degenerate_o);
    check(dut.light_terms_o == g_t.terms, "light_terms_o exact", g_t.terms, dut.light_terms_o);
    check(dut.ndl_clamp_lo_o == g_t.clamp_lo, "ndl_clamp_lo_o exact", g_t.clamp_lo,
          dut.ndl_clamp_lo_o);
    check(dut.ndl_clamp_hi_o == g_t.clamp_hi, "ndl_clamp_hi_o exact", g_t.clamp_hi,
          dut.ndl_clamp_hi_o);
    check(dut.rgb_sat_o == g_t.rgb_sat, "rgb_sat_o exact", g_t.rgb_sat, dut.rgb_sat_o);
    check(dut.cfg_refused_o == g_t.refused, "cfg_refused_o exact", g_t.refused, dut.cfg_refused_o);
    check(dut.nlights_clamped_o == g_t.nl_clamped, "nlights_clamped_o exact", g_t.nl_clamped,
          dut.nlights_clamped_o);
    check(dut.seam_mismatch_o == g_t.seam, "seam_mismatch_o exact", g_t.seam, dut.seam_mismatch_o);

    // THE INDEPENDENT-PATH PAIR. `light_terms_o` counts offers this sequencer
    // got accepted; `engine_shaded_o` counts walks the engine COMPLETED. They
    // are not clocked by one enable, so a reissued light or a dropped result
    // separates them — the defect a value-checking test cannot see.
    check(dut.engine_shaded_o == g_t.terms,
          "the engine ran EXACTLY once per light term — no reissue, no double work", g_t.terms,
          dut.engine_shaded_o);
    check(dut.engine_shaded_o == dut.light_terms_o,
          "sequencer offers and engine completions agree by independent paths", dut.light_terms_o,
          dut.engine_shaded_o);

    // Coverage: a counter total is only evidence if the cases reached it.
    check(g_t.clamp_lo > 20, "coverage: the per-light low clamp was exercised", 1,
          g_t.clamp_lo > 20);
    check(g_t.clamp_hi > 5, "coverage: the per-light high clamp was exercised", 1,
          g_t.clamp_hi > 5);
    check(g_t.rgb_sat > 5, "coverage: the final saturate was exercised", 1, g_t.rgb_sat > 5);
    check(g_t.degen > 5, "coverage: degenerate vertices sampled", 1, g_t.degen > 5);
    check(cov_lit > 20, "coverage: partially lit differential points", 1, cov_lit > 20);
    check(cov_dark > 20, "coverage: dark differential points", 1, cov_dark > 20);
    check(cov_full > 5, "coverage: fully lit differential points", 1, cov_full > 5);

    std::printf(
        "[light] tallies: vtx=%llu terms=%llu engine=%llu degen=%llu lo=%llu hi=%llu sat=%llu "
        "refused=%llu nlclamp=%llu seam=%llu | diff lit=%d dark=%d full=%d\n",
        static_cast<unsigned long long>(g_t.vertices), static_cast<unsigned long long>(g_t.terms),
        static_cast<unsigned long long>(dut.engine_shaded_o),
        static_cast<unsigned long long>(g_t.degen), static_cast<unsigned long long>(g_t.clamp_lo),
        static_cast<unsigned long long>(g_t.clamp_hi), static_cast<unsigned long long>(g_t.rgb_sat),
        static_cast<unsigned long long>(g_t.refused),
        static_cast<unsigned long long>(g_t.nl_clamped), static_cast<unsigned long long>(g_t.seam),
        cov_lit, cov_dark, cov_full);
  }

  dut.final();
  zhao::exit_hard(zhao::report_and_exit("geom_light_directed"));
}
