// stamp_to_bake_laws_directed.cpp -- the two laws TERRAIN.BAKE was missing.
// Owner rulings R15 and R56; spec/terrain_rules.md 9.3.
//
// These are LAWS, not a block: `zref::terrain::stamp_depth` (the art table)
// and `zref::terrain::sheet_texel_for_vertex` (the 64x64 -> 33x33 resample).
// They exist because `zhao_terrain_bake.sv` 29-48 refused to bridge
// SURFACE.STAMP's per-texel results to TERRAIN.BAKE's per-patch dig for a
// stated reason -- "closing that seam needs TWO LAWS THAT DO NOT EXIST
// ANYWHERE IN THIS TREE" -- and a law with no test is a claim.
//
// WHAT IT GUARDS, beyond "the numbers are the numbers":
//   * the table is an ART TABLE and its ENDPOINTS ARE EXACT. Strength n*16
//     returns entry n with no interpolation error at all, which is what makes
//     the sixteen numbers editable: an artist who types -3.25 m gets -3.25 m.
//   * the interpolation is ONE round-half-up on the DELTA, never a rounded sum
//     of rounded terms (qformats 3). Checked against an independent long-double
//     evaluation at every one of the 256 strengths.
//   * the last segment is HELD rather than extrapolated, so 255 is a value
//     somebody chose.
//   * THE DECLARED SEAM ERROR IS THE ERROR THE LAW ACTUALLY HAS. The ruling's
//     fallback is only honest if the number in the spec is measured, so this
//     test MEASURES it: every vertex's texel-centre displacement in quarter
//     cells, and the cross-seam discontinuity between vertex 32 of one patch
//     and vertex 0 of the next.
//   * and the ALTERNATIVE tie-break is shown NOT to help, because the spec
//     claims that and a claim in a spec is still a claim.
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "zref/zref_terrain_page.hpp"

namespace zt = zref::terrain;

namespace {

int g_fail = 0;
int g_checks = 0;

void cke(long long want, long long got, const char* what) {
  ++g_checks;
  if (want != got) {
    ++g_fail;
    std::printf("FAIL: %s -- expected %lld, got %lld\n", what, want, got);
  }
}

void ctrue(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

}  // namespace

int main() {
  // ------------------------------------------------------------------ 1 ----
  // THE TABLE'S ENDPOINTS ARE EXACT. This is what makes the sixteen numbers
  // editable rather than decorative.
  for (int n = 0; n < 16; ++n) {
    cke(zt::kStampDepthTable[n], zt::stamp_depth(static_cast<uint8_t>(n * 16)),
        "strength n*16 returns entry n exactly");
  }
  cke(0, zt::stamp_depth(0), "strength 0 digs nothing");
  ctrue(zt::kStampDepthTable[15] < zt::kStampDepthTable[0],
        "the table digs downward at full strength");
  std::printf("[1] the sixteen entries are exact at their own strengths\n");

  // ------------------------------------------------------------------ 2 ----
  // ONE ROUNDING, ON THE DELTA. Checked against an independent evaluation.
  for (int s = 0; s < 256; ++s) {
    const int hi = s >> 4, fr = s & 15;
    const int32_t a = zt::kStampDepthTable[hi];
    long long want;
    if (fr == 0 || hi == 15) {
      want = a;
    } else {
      const long long d = (static_cast<long long>(zt::kStampDepthTable[hi + 1]) - a) * fr;
      // round-half-up, written differently from the header's shift form so a
      // shared mistake cannot cancel.
      const long long q = static_cast<long long>(std::llround(static_cast<double>(d) / 16.0));
      // llround is round-half-AWAY; on this table every delta is negative and
      // the header rounds half UP (toward +inf), so the two agree except at an
      // exact .5, which is checked explicitly below.
      want = a + q;
      if ((d % 16 == -8) || (d % 16 == 8)) want = a + ((d >= 0) ? ((d + 8) / 16) : -(((-d) + 8) / 16));
    }
    cke(want, zt::stamp_depth(static_cast<uint8_t>(s)), "one round-half-up on the delta");
  }
  cke(zt::kStampDepthTable[15], zt::stamp_depth(255), "the last segment is HELD, not extrapolated");
  std::printf("[2] 256 strengths, one rounding each, against an independent evaluation\n");

  // ------------------------------------------------------------------ 3 ----
  // THE RESAMPLE, AND ITS DECLARED ERROR MEASURED RATHER THAN QUOTED.
  //
  // A texel centre sits at (2i+1)/128 of the patch; a vertex at v/32. In CELLS
  // (32 per patch) that is (2i+1)/4 and v. The displacement is measured in
  // QUARTER CELLS so it stays an integer and no float can hide a wrong sign.
  int interior_disp = 0;
  bool interior_uniform = true;
  for (uint32_t v = 0; v <= 32; ++v) {
    const uint32_t i = zt::sheet_texel_for_vertex(v);
    // quarter-cells: texel centre (2i+1), vertex 4v
    const int disp = static_cast<int>(2 * i + 1) - static_cast<int>(4 * v);
    if (v <= 31) {
      if (v == 0) interior_disp = disp;
      else if (disp != interior_disp) interior_uniform = false;
    } else {
      cke(-1, disp, "vertex 32 is displaced -1/4 cell (the only clamped vertex)");
    }
  }
  cke(1, interior_disp, "every interior vertex is displaced +1/4 cell");
  ctrue(interior_uniform, "the interior displacement is a CONSTANT OFFSET, not a gradient");

  // The seam. Vertex 32 of patch P and vertex 0 of patch P+1 are the SAME
  // world vertex; their two samples are this far apart, in quarter cells.
  const int seam = (static_cast<int>(2 * zt::sheet_texel_for_vertex(0) + 1) + 128) -
                   static_cast<int>(2 * zt::sheet_texel_for_vertex(32) + 1);
  cke(2, seam, "the cross-seam discontinuity is 1/2 cell (2 quarter-cells)");
  std::printf("[3] declared error MEASURED: interior +1/4 cell, far edge -1/4 cell,"
              " seam 1/2 cell\n");

  // ------------------------------------------------------------------ 4 ----
  // THE OTHER TIE-BREAK DOES NOT HELP, which spec 9.3 claims and this proves.
  {
    const auto alt = [](uint32_t v) -> uint32_t { return v == 0 ? 0u : 2u * v - 1u; };
    const int alt_seam = (static_cast<int>(2 * alt(0) + 1) + 128) -
                         static_cast<int>(2 * alt(32) + 1);
    cke(2, alt_seam, "max(2v-1,0) leaves the seam discontinuity at exactly 1/2 cell");
    const int alt_interior = static_cast<int>(2 * alt(16) + 1) - static_cast<int>(4 * 16);
    cke(-1, alt_interior, "the other tie-break merely flips the interior sign");
  }
  std::printf("[4] no integer tie-break removes the seam: the area grid has no edge sample\n");

  // ------------------------------------------------------------------ 5 ----
  // THE TWO LAWS TOGETHER, on a sheet whose strengths encode their own texel.
  {
    uint8_t sheet[64 * 64];
    for (uint32_t j = 0; j < 64; ++j)
      for (uint32_t i = 0; i < 64; ++i) sheet[j * 64 + i] = static_cast<uint8_t>((j * 64 + i) & 0xFF);
    for (uint32_t vj = 0; vj <= 32; vj += 8) {
      for (uint32_t vi = 0; vi <= 32; vi += 8) {
        const uint32_t ti = zt::sheet_texel_for_vertex(vi), tj = zt::sheet_texel_for_vertex(vj);
        cke(zt::stamp_depth(static_cast<uint8_t>((tj * 64 + ti) & 0xFF)),
            zt::stamp_depth_at_vertex(sheet, vi, vj),
            "stamp_depth_at_vertex is the two laws composed and nothing else");
      }
    }
  }
  std::printf("[5] stamp_depth_at_vertex == stamp_depth(sheet[nearest texel])\n");

  std::printf("stamp_to_bake_laws_directed: %d checks, %d failures\n", g_checks, g_fail);
  return g_fail == 0 ? 0 : 1;
}
