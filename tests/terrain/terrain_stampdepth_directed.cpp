// terrain_stampdepth_directed.cpp -- does the LAYER-F READER implement
// spec/terrain_rules.md section 9.3's two laws, or something that looks like
// them?
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS, AND WHY IT IS EXHAUSTIVE RATHER THAN SAMPLED
// ---------------------------------------------------------------------------
// `tests/terrain/stamp_to_bake_laws_directed.cpp` already holds the two laws in
// C++, 306 checks, against an independent long-double evaluation. It proves the
// REFERENCE is right. It says nothing whatever about RTL, because until owner
// ruling R194 (2026-09-20) there was no RTL: the core's entry I32 recorded a
// search of all of `fpga/rtl` for `stamp_depth|kStampDepthTable|
// sheet_texel_for_vertex|depth_table` and found ZERO HITS. The laws lived in
// the spec and in the reference and in no silicon.
//
// `zhao_terrain_stampdepth.sv` is that silicon, and this is the differential.
// The input space is 33 x 33 vertices and 256 strengths -- 278,784 combinations
// if crossed, 1,345 if the two laws are exercised on their own axes -- so there
// is no reason to sample anything. EVERY vertex and EVERY strength is walked,
// and the composed law `zref::terrain::stamp_depth_at_vertex` is walked over a
// real sheet as well, because the two halves agreeing separately is not the
// same claim as the pair agreeing together.
//
// THREE THINGS IT GUARDS THAT A SPOT CHECK WOULD MISS, all of them shapes this
// repository has been bitten by:
//
//  1. THE TIE-BREAK DIRECTION. `sheet_texel_for_vertex` is `min(2v, 63)` and
//     the ONLY vertex that clamps is 32. An off-by-one in the clamp compare
//     (`>` for `>=`) moves exactly one of the 33 vertices and would survive any
//     sampled test that did not happen to pick the far edge. Case 1 asserts the
//     clamp fires at 32 AND that it fires nowhere else, which are two claims.
//
//  2. THE SIGN OF THE ROUNDING. The art table is almost entirely NEGATIVE, and
//     the oracle's interpolation rounds SYMMETRICALLY about zero
//     (`-(((-d) + 8) >> 4)`), not half-up. A part-select in SystemVerilog is
//     UNSIGNED whatever its parent's signedness, so the characteristic RTL
//     defect here turns a small negative delta into a huge positive one and
//     RAISES ground where the game meant to dig it. Case 2 walks all 256
//     strengths, and case 2b asserts the direction of the whole column.
//
//  3. THE ENDPOINTS ARE EXACT. Strength n*16 must return entry n with no
//     interpolation error at all -- that is what makes the sixteen numbers
//     EDITABLE (CLAUDE.md rule 6: an artist who types -3.25 m gets -3.25 m).
//     Case 3 asserts it at all sixteen, and case 3b asserts the RTL holds the
//     LAST SEGMENT rather than extrapolating past entry 15, so strength 255 is
//     a value somebody chose.
//
// AND ONE THING IT DELIBERATELY DOES NOT DO. It does not check that the sixteen
// depths are the RIGHT depths. They are provisional art values and only the
// owner's eye, in scene, at final resolution, can settle them (terrain_rules
// section 9.3(a)). What it checks is that the RTL and `zref::terrain::
// kStampDepthTable` hold the SAME sixteen -- so that when the owner moves one,
// the silicon and the oracle move together or the suite goes red.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_terrain_stampdepth.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_page.hpp"

namespace zt = zref::terrain;

namespace {

int g_fail = 0;
long long g_checks = 0;

void cke(long long want, long long got, const char* what) {
  ++g_checks;
  if (want != got) {
    ++g_fail;
    if (g_fail <= 20)
      std::printf("FAIL: %s -- expected %lld, got %lld\n", what, want, got);
  }
}

void ctrue(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    if (g_fail <= 20) std::printf("FAIL: %s\n", what);
  }
}

// The DUT is pure combinational: no clock, no reset, no handshake. Settling it
// is one eval().
int32_t rtl_depth_fx16(Vzhao_terrain_stampdepth& dut, uint32_t vi, uint32_t vj,
                       uint8_t strength) {
  dut.vi_i = static_cast<uint8_t>(vi);
  dut.vj_i = static_cast<uint8_t>(vj);
  dut.strength_i = strength;
  dut.eval();
  return static_cast<int32_t>(dut.depth_fx16_o);
}

// spec/qformats.md: fx16 -> height16 is rescale(x, 8), ROUND-HALF-UP, which is
// an arithmetic `(x + 128) >> 8`. Written here as the reference half of the
// comparison so the RTL's `{{7{h_round[32]}}, h_round[32:8]}` is checked
// against the law rather than against itself.
int32_t ref_height16(int32_t fx16) {
  const int64_t t = static_cast<int64_t>(fx16) + 128;
  return static_cast<int32_t>(t >> 8);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_stampdepth dut;

  // ------------------------------------------------------------------ 1 ----
  // section 9.3(b): THE ADDRESS LAW, every vertex of the lattice, both axes.
  //
  // `texel_o` is the sheet's own encoding, j*64 + i in scan order, so the
  // check is on the COMPOSED address and not on two indices the test has
  // re-assembled -- reassembling them here would be a second implementation of
  // the thing under test.
  std::printf("[1] sheet_texel_for_vertex, all 33 x 33 vertices\n");
  {
    int clamped_vertices = 0;
    for (uint32_t vj = 0; vj < 33; ++vj) {
      for (uint32_t vi = 0; vi < 33; ++vi) {
        dut.vi_i = static_cast<uint8_t>(vi);
        dut.vj_i = static_cast<uint8_t>(vj);
        dut.strength_i = 0;
        dut.eval();
        const uint32_t ti = zt::sheet_texel_for_vertex(vi);
        const uint32_t tj = zt::sheet_texel_for_vertex(vj);
        cke(static_cast<long long>(tj * zt::kSheetEdge + ti),
            static_cast<long long>(dut.texel_o),
            "texel_o == sheet_texel_for_vertex(vj)*64 + sheet_texel_for_vertex(vi)");
        if (ti == zt::kSheetEdge - 1 && 2 * vi != zt::kSheetEdge - 1) ++clamped_vertices;
      }
    }
    // THE CLAMP FIRES EXACTLY ONCE PER AXIS. 33 rows x the single vertex 32 on
    // the i axis = 33 hits, and no more: an off-by-one in the compare would
    // move this count, which a sampled test could not see.
    cke(33, clamped_vertices, "exactly one clamped vertex per row -- vertex 32, the far edge");
  }

  // ------------------------------------------------------------------ 2 ----
  // section 9.3(a): THE ART TABLE, all 256 strengths, against the oracle.
  std::printf("[2] stamp_depth, all 256 strengths\n");
  {
    for (int s = 0; s < 256; ++s) {
      const int32_t want = zt::stamp_depth(static_cast<uint8_t>(s));
      const int32_t got = rtl_depth_fx16(dut, 0, 0, static_cast<uint8_t>(s));
      cke(want, got, "depth_fx16_o == zref::terrain::stamp_depth(strength)");
    }
  }

  // ----------------------------------------------------------------- 2b ----
  // THE WHOLE COLUMN DIGS. A sign error in the interpolation -- the unsigned
  // part-select trap -- turns the deltas positive and every scar RAISES ground.
  // This asserts the direction independently of the values, so it stays true
  // when the owner re-authors the sixteen numbers (unless the owner authors a
  // Volcano table, which is legal and would correctly fail this and require the
  // line to be re-authored with it).
  std::printf("[2b] every non-zero strength digs DOWN with the shipped table\n");
  {
    for (int s = 1; s < 256; ++s) {
      const int32_t got = rtl_depth_fx16(dut, 0, 0, static_cast<uint8_t>(s));
      ctrue(got < 0, "a non-zero strength produces a negative (digging) depth");
    }
    cke(0, rtl_depth_fx16(dut, 0, 0, 0), "strength 0 digs nothing at all");
  }

  // ------------------------------------------------------------------ 3 ----
  // THE ENDPOINTS ARE EXACT -- the property that makes the table editable.
  std::printf("[3] the sixteen entries are reachable EXACTLY at strength n*16\n");
  {
    for (int n = 0; n < 16; ++n) {
      cke(zt::kStampDepthTable[n], rtl_depth_fx16(dut, 0, 0, static_cast<uint8_t>(n * 16)),
          "entry n is returned exactly at strength n*16");
    }
  }

  // ----------------------------------------------------------------- 3b ----
  // THE LAST SEGMENT IS HELD. 241..255 must all return entry 15 and not run off
  // the end of the table into entry 0 -- the RTL reads `depth_entry(s_hi + 1)`
  // unconditionally and the `held_segment` select is what makes the wrap
  // unreachable. If that select were dropped, strength 241 would interpolate
  // from -10.00 m toward 0.00 m and the deepest stamp in the game would get
  // SHALLOWER as it got stronger.
  std::printf("[3b] the last segment is HELD, not extrapolated and not wrapped\n");
  {
    for (int s = 240; s < 256; ++s) {
      cke(zt::kStampDepthTable[15], rtl_depth_fx16(dut, 0, 0, static_cast<uint8_t>(s)),
          "strengths 240..255 all return entry 15");
    }
  }

  // ------------------------------------------------------------------ 4 ----
  // fx16 -> height16, the domain `zhao_terrain_bake_v2`'s scar arithmetic
  // works in. The RTL must apply the SAME operator `delta_g` applies, or the
  // sheet mode and the disc mode would disagree about what half an LSB is.
  std::printf("[4] depth_h16_o == rescale(depth_fx16_o, 8), round-half-up\n");
  {
    for (int s = 0; s < 256; ++s) {
      dut.vi_i = 0;
      dut.vj_i = 0;
      dut.strength_i = static_cast<uint8_t>(s);
      dut.eval();
      const int32_t fx = zt::stamp_depth(static_cast<uint8_t>(s));
      cke(ref_height16(fx), static_cast<int32_t>(dut.depth_h16_o),
          "depth_h16_o is the fx16 depth rescaled by 8 with round-half-up");
    }
  }

  // ------------------------------------------------------------------ 5 ----
  // COVERAGE. `covered_o` is what arms bake's no_bake clamp and its height16
  // rails at this vertex -- the role `covers` plays on the parametric-disc
  // path -- so it is a separate claim from the depth being zero.
  std::printf("[5] covered_o is (strength != 0)\n");
  {
    for (int s = 0; s < 256; ++s) {
      dut.vi_i = 0;
      dut.vj_i = 0;
      dut.strength_i = static_cast<uint8_t>(s);
      dut.eval();
      cke(s != 0 ? 1 : 0, dut.covered_o ? 1 : 0, "covered_o == (strength != 0)");
    }
  }

  // ------------------------------------------------------------------ 6 ----
  // THE TWO LAWS COMPOSED, over a real sheet, at every vertex.
  //
  // `zref::terrain::stamp_depth_at_vertex` is the composition and is the oracle
  // `zhao_terrain_bake_v2`'s per-vertex depth mode is written against
  // (terrain_rules section 9.3(c)'s closing line says so in as many words). The
  // sheet below is a DISC whose rim crosses the half-cell band, because that is
  // where the two laws differ from a vertex-aligned read and therefore the only
  // place the composition can be wrong in an interesting way: a flat sheet
  // would pass with the address generator removed entirely.
  std::printf("[6] stamp_depth_at_vertex over rim-crossing disc sheets\n");
  {
    static uint8_t sheet[zt::kSheetEdge * zt::kSheetEdge];

    // Paint a disc of radius `r` centred at (cx, cz) patch metres, strength 128
    // (-3.25 m, a spell impact). The coverage test is a THRESHOLD, exactly as
    // `zref::surface::covers` is -- which is section 9.3(c)'s whole point: when
    // the half-cell offset moves a sample across the rim, strength does not
    // change by a little, it changes from 0 to the stamp's full value.
    auto paint = [&](double cx, double cz, double r) {
      for (uint32_t tj = 0; tj < zt::kSheetEdge; ++tj) {
        for (uint32_t ti = 0; ti < zt::kSheetEdge; ++ti) {
          // texel centre in patch metres: (2i+1)/128 of a 32 m patch.
          const double wx = (2.0 * ti + 1.0) / 128.0 * 32.0;
          const double wz = (2.0 * tj + 1.0) / 128.0 * 32.0;
          const double d2 = (wx - cx) * (wx - cx) + (wz - cz) * (wz - cz);
          sheet[tj * zt::kSheetEdge + ti] = (d2 <= r * r) ? 128 : 0;
        }
      }
    };

    // Present the vertex, read back the address the RTL chose, fetch THAT texel
    // from the sheet, present it. This is exactly how bake and the page server
    // will be wired, so the test drives the same loop.
    auto walk = [&](const char* what) {
      int touched = 0;
      for (uint32_t vj = 0; vj < 33; ++vj) {
        for (uint32_t vi = 0; vi < 33; ++vi) {
          dut.vi_i = static_cast<uint8_t>(vi);
          dut.vj_i = static_cast<uint8_t>(vj);
          dut.eval();
          const uint32_t addr = dut.texel_o;
          ctrue(addr < zt::kSheetEdge * zt::kSheetEdge, "the address is inside the sheet");
          dut.strength_i = sheet[addr];
          dut.eval();
          cke(zt::stamp_depth_at_vertex(sheet, vi, vj), static_cast<int32_t>(dut.depth_fx16_o),
              "the RTL loop reproduces stamp_depth_at_vertex exactly");
          if (dut.covered_o) ++touched;
        }
      }
      std::printf("    %s: %d of 1089 vertices under the disc\n", what, touched);
      return touched;
    };

    // (a) A 9 m disc WHOLLY INSIDE the patch -- centre (16,16), so 7..25 m on
    //     both axes. ANTI-VACUITY: its area is pi*81 = 254 m^2 of a 32x32 m
    //     patch, so at a 1 m lattice it must touch roughly 250 vertices. An
    //     empty sheet would make every check above pass on zeros and prove
    //     nothing whatever, which is the failure this bound exists to catch.
    paint(16.0, 16.0, 9.0);
    const int touched_interior = walk("interior disc");
    ctrue(touched_interior > 200 && touched_interior < 320,
          "the interior disc covers about a quarter of the lattice -- not vacuous");

    // (b) THE R194 PLACEMENT: a 9 m disc whose rim is TANGENT to the patch
    //     EDGE, the worst case `tools/terrain/seam_dig_render.cpp` could
    //     construct and the one the owner looked at on
    //     `reports/terrain-seam-dig/seam_dig_contact.png` before ruling
    //     "Shipped is fine. Slightly different but not off." The far-edge
    //     vertex 32 is the only CLAMPED one, so this is the placement where the
    //     clamp and the rim interact -- and it is exactly where a wrong
    //     tie-break would show up and nowhere else.
    paint(23.0, 16.0, 9.0);
    const int touched_tangent = walk("seam-tangent disc");
    ctrue(touched_tangent > 150, "the tangent disc reaches the far edge -- not vacuous");
    // The rim reaching column 32 is the whole point of this placement; assert
    // it rather than assume it.
    {
      int far_edge_touched = 0;
      for (uint32_t vj = 0; vj < 33; ++vj) {
        dut.vi_i = 32;
        dut.vj_i = static_cast<uint8_t>(vj);
        dut.eval();
        dut.strength_i = sheet[dut.texel_o];
        dut.eval();
        if (dut.covered_o) ++far_edge_touched;
      }
      ctrue(far_edge_touched > 0,
            "the tangent disc actually reaches vertex 32, the clamped column");
      std::printf("    seam-tangent disc touches %d of 33 far-edge vertices\n", far_edge_touched);
    }
  }

  if (g_fail == 0) {
    std::printf(
        "PASS terrain_stampdepth_directed -- %lld checks. The address law, the "
        "art table, the held last segment, the height16 rescale, coverage and "
        "the composed law all match zref exactly.\n",
        g_checks);
    zhao::exit_hard(0);
  }
  std::printf("FAILED terrain_stampdepth_directed -- %d of %lld check(s)\n", g_fail, g_checks);
  zhao::exit_hard(1);
}
