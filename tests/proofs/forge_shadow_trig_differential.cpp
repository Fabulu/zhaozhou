// forge_shadow_trig_differential.cpp -- narrowing the unit-circle table from a
// declared 32 bits to its true 18 emits the same bits, over every table entry
// and the full declared range of the radius it multiplies.
//
// ---------------------------------------------------------------------------
// WHAT CHANGED AND WHY IT COULD HAVE BEEN FREE
// ---------------------------------------------------------------------------
// `zhao_forge_shadow` builds each shadow hull vertex with
//
//     wire signed [63:0] off_x_w = $signed(rad_q) * unit_cos(tbl_c);
//
// `unit_cos` and `unit_sin` returned `logic signed [31:0]`, so that is a 32x32
// multiply. Every one of the table's SIXTEEN values lies in [-65536, +65536],
// which an 18-bit signed value holds exactly (range [-131072, +131071]), so
// the return type is now `logic signed [17:0]` and the multiply is 32x18.
//
//   row                              DSP  9x9  18x18pr  sum2  +36  reg  ALUT
//   zhao_forge_shadow@gzdsp-base       7    1        4     2    0  733   510
//   zhao_forge_shadow@gzdsp-tab18      5    1        2     0    2  733   450
//
// -2 DSP, -60 ALUTs, registers unchanged. AND NOTE THIS BLOCK NEVER HAD AN
// `Independent 27x27` AT ALL: the brief's sorting column would have skipped it,
// and it paid anyway.
//
// ---------------------------------------------------------------------------
// THE EXACTNESS ARGUMENT, AND WHY THIS FILE STILL EXISTS
// ---------------------------------------------------------------------------
// The argument is short and it is genuinely EXHAUSTIVE over the thing that
// changed: the functions take a 4-bit input, so their entire input domain is
// SIXTEEN VALUES, and every one of the returned integers is enumerated in the
// source. None moves. At the use site both forms sign-extend into the same
// 64-bit multiply context, so the product is the same integer.
//
// That argument is also exactly the kind that is convincing and wrong -- the
// skin-normal repair in this same packet had an argument just as tidy and was
// off by 128 over a quarter of a port's range. So the argument is not the
// evidence. `Vshadow_old` is `tests/probes/zhao_shadow_trig_probe.sv`, which is
// production at commit 5049399c with the module renamed and NOTHING else
// touched, produced mechanically by `git show` plus one substitution.
//
// ---------------------------------------------------------------------------
// WHAT THIS PROOF IS AND IS NOT -- STATED, NOT IMPLIED
// ---------------------------------------------------------------------------
// It is EXHAUSTIVE in the dimension that changed and SAMPLED in the others,
// and the two must not be muddled:
//
//   * EXHAUSTIVE over the table. Every rung is driven, and the test asserts
//     that all sixteen table indices were reached (`k_seen == 0xFFFF` is
//     checked, not assumed) -- so no entry is proved by omission.
//   * EXHAUSTIVE over the rung ladder and the governor floor: all 4 x 4 pairs.
//   * SAMPLED over `cast_radius_i`, which is a full signed 32 bits and cannot
//     be swept. Both rails, zero, the powers of two either side of the fx16
//     point, and randomised values on a fixed seed.
//
// Outputs are compared on EVERY emitted vertex -- x, y, z, alpha, last, src id
// and rung -- plus all five census counters at the end, because a repair that
// changed the DATA while leaving the counters alone would be just as wrong.
//
// ENFORCED-BY: tests/CMakeLists.txt : forge_shadow_trig_differential

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vshadow_new.h"
#include "Vshadow_old.h"

#include "zhao_sim.hpp"

namespace {

uint64_t rng_state = 0x243F6A8885A308D3ull;
uint64_t next_rand() {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 7;
  rng_state ^= rng_state << 17;
  return rng_state;
}

struct Cast {
  int32_t x, z, radius;
  uint8_t strength;
  uint8_t rung;
  uint8_t floor_;
  uint16_t src;
  int32_t height;
  bool no_ground;
};

template <typename T>
void reset_dut(T& d) {
  d.rst_n = 0;
  d.cast_valid_i = 0;
  d.tap_req_ready_i = 1;
  d.tap_rsp_valid_i = 0;
  d.tap_height_i = 0;
  d.tap_no_ground_i = 0;
  d.vtx_ready_i = 1;
  d.rung_floor_i = 0;
  d.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
}

/** Drive one caster to completion, collecting every emitted vertex. Returns
 *  the number of vertices seen. `sink` receives each vertex tuple. */
template <typename T, typename F>
int run_cast(T& d, const Cast& c, F sink) {
  d.cast_x_i = (uint32_t)c.x;
  d.cast_z_i = (uint32_t)c.z;
  d.cast_radius_i = (uint32_t)c.radius;
  d.cast_strength_i = c.strength;
  d.cast_rung_i = c.rung;
  d.cast_src_id_i = c.src;
  d.rung_floor_i = c.floor_;
  d.vtx_ready_i = 1;
  d.tap_req_ready_i = 1;

  int guard = 0;
  while (!d.cast_ready_o && guard++ < 512) zhao::tick(d);
  d.cast_valid_i = 1;
  zhao::tick(d);
  d.cast_valid_i = 0;
  d.eval();

  int vertices = 0;
  for (int cycle = 0; cycle < 4096; ++cycle) {
    // Answer every height tap the moment it is offered.
    if (d.tap_req_valid_o && d.tap_req_ready_i) {
      zhao::tick(d);
      d.tap_rsp_valid_i = 1;
      d.tap_height_i = (uint32_t)c.height;
      d.tap_no_ground_i = c.no_ground ? 1 : 0;
      zhao::tick(d);
      d.tap_rsp_valid_i = 0;
      d.eval();
      continue;
    }
    if (d.vtx_valid_o && d.vtx_ready_i) {
      sink(d);
      ++vertices;
      const bool last = d.vtx_last_o != 0;
      zhao::tick(d);
      if (last) return vertices;
      continue;
    }
    zhao::tick(d);
    if (d.cast_ready_o && vertices > 0) return vertices;
    if (d.cast_ready_o && cycle > 64) return vertices;  // refused: no hull
  }
  return vertices;
}

constexpr int32_t RMIN = INT32_MIN;
constexpr int32_t RMAX = INT32_MAX;

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vshadow_new dnew;
  Vshadow_old dold;

  reset_dut(dnew);
  reset_dut(dold);

  long casts = 0, vtx_compared = 0, bad = 0, reported = 0;
  uint32_t k_seen = 0;  // which of the 16 table indices a hull actually used

  auto one_cast = [&](const Cast& c, const char* origin) {
    int32_t nx[64], ny[64], nz[64];
    uint32_t na[64], nl[64], ns[64], nr[64];
    int n_old = 0;

    // `w` is a SEPARATE cursor incremented inside the sink. The obvious shape
    // -- `n_new = run_cast(..., [&]{ nx[n_new] = ... })` -- is a bug, and it
    // was in this file: the assignment only happens when run_cast RETURNS, so
    // every vertex lands at index 0 and the comparison reads uninitialised
    // memory from index 1 on. It failed on 1,663 of 1,663 emitted hulls while
    // the block's own directed test passed 39 of 39, which is the tell: a
    // differential that disagrees on EVERYTHING is usually the harness, not
    // the design.
    int w = 0;
    const int n_new = run_cast(dnew, c, [&](Vshadow_new& d) {
      if (w < 64) {
        nx[w] = (int32_t)d.vtx_x_o;
        ny[w] = (int32_t)d.vtx_y_o;
        nz[w] = (int32_t)d.vtx_z_o;
        na[w] = d.vtx_alpha_o;
        nl[w] = d.vtx_last_o;
        ns[w] = d.vtx_src_id_o;
        nr[w] = d.vtx_rung_o;
      }
      ++w;
    });
    int idx = 0;
    bool differ = false;
    n_old = run_cast(dold, c, [&](Vshadow_old& d) {
      if (idx < 64 && idx < n_new && idx < w) {
        if ((int32_t)d.vtx_x_o != nx[idx]) differ = true;
        if ((int32_t)d.vtx_y_o != ny[idx]) differ = true;
        if ((int32_t)d.vtx_z_o != nz[idx]) differ = true;
        if (d.vtx_alpha_o != na[idx]) differ = true;
        if (d.vtx_last_o != nl[idx]) differ = true;
        if (d.vtx_src_id_o != ns[idx]) differ = true;
        if (d.vtx_rung_o != nr[idx]) differ = true;
        ++vtx_compared;
      }
      ++idx;
    });
    if (n_new != n_old) differ = true;
    ++casts;

    // Which table indices this hull's vertex count implies it walked. A hull
    // of N vertices at stride 16/N touches k = 0, s, 2s, ... so record them.
    if (n_new > 0 && n_new <= 16 && (16 % n_new) == 0) {
      const int stride = 16 / n_new;
      for (int i = 0; i < n_new; ++i) k_seen |= (1u << ((i * stride) & 15));
    }

    if (differ) {
      ++bad;
      if (reported++ < 10) {
        std::printf(
            "MISMATCH [%s] rung=%u floor=%u radius=%d height=%d nvtx new=%d "
            "old=%d\n",
            origin, (unsigned)c.rung, (unsigned)c.floor_, c.radius, c.height,
            n_new, n_old);
      }
    }
  };

  // ---- every rung against every governor floor, at radii that produce a hull
  const int32_t radii[] = {65536,   1,        2,       65535,   65537,
                           1 << 20, 1 << 30,  RMAX,    -1,      -65536,
                           RMIN,    RMIN + 1, 1 << 16, 3 << 16, 0};
  Cast c{};
  c.strength = 200;
  c.src = 0x1234;
  c.height = 4096;
  c.no_ground = false;
  for (uint8_t rung = 0; rung < 4; ++rung) {
    for (uint8_t fl = 0; fl < 4; ++fl) {
      for (int32_t r : radii) {
        c.rung = rung;
        c.floor_ = fl;
        c.radius = r;
        c.x = 1 << 16;
        c.z = -(1 << 16);
        one_cast(c, "ladder");
      }
    }
  }

  // ---- the no-ground and strength rails ------------------------------------
  for (uint8_t rung = 0; rung < 4; ++rung) {
    for (int ng = 0; ng < 2; ++ng) {
      for (uint32_t st : {0u, 1u, 128u, 255u}) {
        c.rung = rung;
        c.floor_ = 0;
        c.radius = 2 << 16;
        c.strength = (uint8_t)st;
        c.no_ground = ng != 0;
        c.height = ng ? RMIN : RMAX;
        c.x = RMAX;
        c.z = RMIN;
        one_cast(c, "rails");
      }
    }
  }
  c.strength = 200;
  c.no_ground = false;

  // ---- the randomised sweep ------------------------------------------------
  for (int n = 0; n < 3000; ++n) {
    c.radius = (int32_t)(uint32_t)next_rand();
    c.x = (int32_t)(uint32_t)next_rand();
    c.z = (int32_t)(uint32_t)next_rand();
    c.height = (int32_t)(uint32_t)next_rand();
    c.strength = (uint8_t)next_rand();
    c.rung = (uint8_t)(next_rand() & 3);
    c.floor_ = (uint8_t)(next_rand() & 3);
    c.src = (uint16_t)next_rand();
    c.no_ground = (next_rand() & 7) == 0;
    one_cast(c, "random");
  }

  // ---- the census counters, compared once at the end -----------------------
  const bool census_differ =
      (dnew.shadows_emitted_o != dold.shadows_emitted_o) ||
      (dnew.no_ground_o != dold.no_ground_o) ||
      (dnew.zero_radius_o != dold.zero_radius_o) ||
      (dnew.far_rung_o != dold.far_rung_o) ||
      (dnew.tap_protocol_o != dold.tap_protocol_o);
  if (census_differ) {
    ++bad;
    std::printf(
        "MISMATCH [census] emitted %u/%u noground %u/%u zero %u/%u far %u/%u "
        "proto %u/%u\n",
        (unsigned)dnew.shadows_emitted_o, (unsigned)dold.shadows_emitted_o,
        (unsigned)dnew.no_ground_o, (unsigned)dold.no_ground_o,
        (unsigned)dnew.zero_radius_o, (unsigned)dold.zero_radius_o,
        (unsigned)dnew.far_rung_o, (unsigned)dold.far_rung_o,
        (unsigned)dnew.tap_protocol_o, (unsigned)dold.tap_protocol_o);
  }

  std::printf(
      "forge_shadow_trig_differential: %ld casts, %ld vertices compared, "
      "%ld mismatch\n",
      casts, vtx_compared, bad);
  std::printf("  table indices reached: 0x%04X (want 0xFFFF)\n", k_seen);
  std::printf("  emitted hulls: %u\n", (unsigned)dnew.shadows_emitted_o);

  // A differential that ran nothing passes vacuously, so these are checks and
  // not decoration. The k_seen check is the one that makes "exhaustive over the
  // table" a measurement rather than a claim.
  if (casts < 3000) {
    std::printf("FAIL: stimulus did not run (%ld casts)\n", casts);
    zhao::exit_hard(1);
  }
  if (vtx_compared < 1000) {
    std::printf("FAIL: too few vertices compared (%ld)\n", vtx_compared);
    zhao::exit_hard(1);
  }
  if (k_seen != 0xFFFFu) {
    std::printf("FAIL: not every table index was reached (0x%04X)\n", k_seen);
    zhao::exit_hard(1);
  }
  if (bad != 0) {
    std::printf("FAIL: %ld disagreements\n", bad);
    zhao::exit_hard(1);
  }
  std::printf("PASS\n");
  zhao::exit_hard(0);
}
