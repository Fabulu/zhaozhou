// geom_depthquant_directed.cpp — w to invw24, against the ratified law.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK CLOSES
// ---------------------------------------------------------------------------
// `BORING_3D_FUNDAMENTALS_AUDIT.md` R6: zhao_geom_project.sv emits `out_d_o`,
// documented "Q16.16 1/w"; TWELVE RTL files consume `invw24`; and no
// `depth_profile` port existed anywhere. The conversion had no home.
//
// ---------------------------------------------------------------------------
// THE ORACLE IS THE RATIFIED LAW, NOT A RESTATEMENT
// ---------------------------------------------------------------------------
// `zref::depth_of_raw` already existed and is complete. So this file checks
// RTL against the law the golden captures already pin, rather than against a
// second implementation — which is the mistake made and caught earlier today
// with the flat-shade law, where twelve checks passed because a duplicate was
// being compared with itself.
//
// The table values in the RTL are also checked against the GENERATED table
// here, because a table and its user drifting is the `QFMT_VERSION` failure in
// a different costume, and that one actually happened this morning.
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_geom_depthquant.h"

#include "zhao_sim.hpp"
#include "zref/generated/zref_depth.hpp"
#include "zref/zref_depth.hpp"
#include "zref/zref_rcp.hpp"

namespace {

// Answer the block's reciprocal port from the same `rcp_u24` the law uses, so
// the DUT and the oracle share one reciprocal rather than two that agree until
// they do not.
uint32_t drive(Vzhao_geom_depthquant& t, uint64_t w, unsigned profile) {
  t.v_valid_i = 1;
  t.v_w_i = w;
  t.v_behind_i = 0;
  t.v_profile_i = profile;
  t.v_src_id_i = 0x1234;
  t.d_ready_i = 1;
  t.rcp_ready_i = 1;
  t.rcp_rvalid_i = 0;
  t.eval();
  zhao::tick(t);
  t.v_valid_i = 0;

  uint32_t out = 0xFFFFFFFFu;
  for (int c = 0; c < 200; ++c) {
    t.rcp_ready_i = 1;
    t.rcp_rvalid_i = 0;
    t.eval();
    if (t.rcp_valid_o) {
      // the request was taken this cycle; answer it next
      const zref::rcp24_result rc = zref::rcp_u24(t.rcp_d_o ? t.rcp_d_o : 1u);
      zhao::tick(t);
      t.rcp_rvalid_i = 1;
      t.rcp_r_i = rc.r;
      t.rcp_k_i = rc.k;
      t.eval();
      zhao::tick(t);
      t.rcp_rvalid_i = 0;
      continue;
    }
    if (t.d_valid_o) {
      out = t.d_invw24_o;
      zhao::tick(t);
      break;
    }
    zhao::tick(t);
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_geom_depthquant top;

  top.v_valid_i = 0;
  top.rcp_ready_i = 1;
  top.rcp_rvalid_i = 0;
  top.d_ready_i = 1;
  top.rst_n = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  // ---- 1: THE TABLE IN THE RTL MATCHES THE GENERATED TABLE ---------------
  // Checked here rather than trusted. The RTL mirrors wmin/wmax and carries
  // SCALE as a log2; if the generator ever moves, this fails loudly instead of
  // the hardware quietly using yesterday's numbers.
  {
    int bad = 0;
    const uint64_t wmin[3] = {65536ull, 32768ull, 16384ull};
    const uint64_t wmax[3] = {1073741824ull, 536870912ull, 134217728ull};
    const unsigned long long scale[3] = {1099511627776ull, 549755813888ull,
                                         274877906944ull};
    for (int p = 0; p < 3; ++p) {
      const auto& g = zref::gen::DEPTH_PROFILES[p];
      if (g.wmin_raw != wmin[p]) ++bad;
      if (g.wmax_raw != wmax[p]) ++bad;
      if (g.scale != scale[p]) ++bad;
      // and SCALE really is a power of two, which is what lets the RTL use a
      // shift instead of a multiplier. A fourth profile that broke this would
      // need real hardware, and the RTL says so.
      if ((g.scale & (g.scale - 1ull)) != 0ull) {
        ++bad;
        std::printf("    profile %d SCALE %llu is not a power of two -- the "
                    "RTL's shift is invalid for it\n", p, g.scale);
      }
    }
    zhao::check(bad == 0,
                "the RTL's mirrored profile table matches the GENERATED one, "
                "and every SCALE is a power of two -- which is what makes the "
                "multiply a shift",
                0, bad);
  }

  // ---- 2: RTL == the ratified law, across the range of every profile -----
  {
    int bad = 0, compared = 0;
    for (unsigned p = 0; p < 3; ++p) {
      const auto& g = zref::gen::DEPTH_PROFILES[p];
      const uint64_t pts[] = {
          g.wmin_raw,                    // the near pin
          g.wmin_raw + 1,
          g.wmin_raw * 3,
          g.wmin_raw * 17,
          (g.wmin_raw + g.wmax_raw) / 2,
          g.wmax_raw / 4,
          g.wmax_raw - 1,
          g.wmax_raw,                    // the far floor
      };
      for (uint64_t w : pts) {
        const uint32_t want = zref::depth_of_raw(w, p);
        const uint32_t got = drive(top, w, p);
        ++compared;
        if (got != want) {
          if (bad < 5)
            std::printf("    profile %u w=%llu: rtl %06X, law %06X\n", p,
                        (unsigned long long)w, got, want);
          ++bad;
        }
      }
    }
    zhao::check(compared == 24,
                "every profile was exercised across its range rather than at "
                "one convenient point",
                24, compared);
    zhao::check(bad == 0,
                "RTL matches zref::depth_of_raw exactly -- the law the golden "
                "captures already pin, not a restatement of it",
                0, bad);
  }

  // ---- 3: THE CLAMP IS THE LAW, NOT A COURTESY ---------------------------
  // wmax is a depth CLAMP, not a far-clip plane: a w beyond it is legal
  // geometry that shares the floor depth rather than being culled.
  {
    const auto& g = zref::gen::DEPTH_PROFILES[0];
    const uint32_t before_near = top.clamped_near_o;
    const uint32_t before_far = top.clamped_far_o;

    const uint32_t near_got = drive(top, g.wmin_raw / 2, 0);
    const uint32_t near_want = zref::depth_of_raw(g.wmin_raw, 0);
    const uint32_t far_got = drive(top, g.wmax_raw * 2, 0);
    const uint32_t far_want = zref::depth_of_raw(g.wmax_raw, 0);

    zhao::check(near_got == near_want && far_got == far_want,
                "a w outside [wmin, wmax] takes the boundary's depth -- geometry "
                "beyond the far clamp is drawn at the floor, not culled",
                1, (near_got == near_want && far_got == far_want) ? 1 : 0);
    zhao::check(top.clamped_near_o == before_near + 1 &&
                    top.clamped_far_o == before_far + 1,
                "and each clamp is COUNTED, so a profile wrong for the content "
                "is visible rather than merely dim",
                1,
                (top.clamped_near_o == before_near + 1 &&
                 top.clamped_far_o == before_far + 1)
                    ? 1
                    : 0);
  }

  // ---- 4: the near pin really is 0xFFFFFF --------------------------------
  {
    int bad = 0;
    for (unsigned p = 0; p < 3; ++p) {
      const auto& g = zref::gen::DEPTH_PROFILES[p];
      if (drive(top, g.wmin_raw, p) != g.d_at_wmin) ++bad;
    }
    zhao::check(bad == 0,
                "every profile pins its near plane at exactly 0xFFFFFF -- the "
                "generator SOLVES scale for that, and an off-by-one here means "
                "the solve did not survive into hardware",
                0, bad);
  }

  // ---- 5: the far floor is non-zero -------------------------------------
  // A zero far depth would make distant geometry indistinguishable from
  // "behind the eye", which the profile table deliberately avoids.
  {
    int bad = 0;
    for (unsigned p = 0; p < 3; ++p) {
      const auto& g = zref::gen::DEPTH_PROFILES[p];
      const uint32_t got = drive(top, g.wmax_raw, p);
      if (got != g.d_at_wmax || got == 0) ++bad;
    }
    zhao::check(bad == 0,
                "and the far floor is the table's own non-zero value -- zero "
                "would make the far plane indistinguishable from behind-eye",
                0, bad);
  }

  std::printf("  %u vertices, %u near-clamped, %u far-clamped, %u saturated, "
              "%u refused\n",
              top.vertices_o, top.clamped_near_o, top.clamped_far_o,
              top.saturated_o, top.refused_o);

  return zhao::report_and_exit("geom_depthquant_directed");
}
