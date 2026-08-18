// zref_terrain_normals.hpp — the TERRAIN.NORMALS oracle.
//
// This is a THIN view onto an existing ratified law, not a second
// implementation of it. `reference/src/zrender/terrain.cpp`'s `shade_flat_tri`
// already computes the deformed-surface face normal; it just does not expose it,
// because it consumes the normal immediately in a dot product. This header
// exposes exactly that arithmetic and nothing else, so "RTL == oracle" means
// "RTL == the law the golden captures already pin".
//
// The Q-format algebra and the defect it carries are quoted in
// fpga/rtl/terrain/zhao_terrain_normals.sv. In one line: the lanes are Q32.32
// and the shift back to Q16.16 is rescale(., 16), NOT 32. At 32 a sub-metre
// lattice shades solid black, which is a bug this project already paid for once.

#pragma once

#include <cstdint>

#include "zref/zref_fixp.hpp"

namespace zref {
namespace terrain {

/** One vertex of the deformed surface, fx16 world units. */
struct NormalVertex {
  int32_t x = 0;
  int32_t y = 0;
  int32_t z = 0;
};

/** The unnormalised Q16.16 face normal, plus the zero-area verdict. */
struct FaceNormal {
  int32_t x = 0;
  int32_t y = 0;
  int32_t z = 0;
  bool degenerate = false;  // all three lanes rescale to exactly zero
};

/**
 * The face normal of triangle (a, b, c), as `shade_flat_tri` computes it.
 *
 * Deliberately NOT normalised: the reference divides by |n| only at the moment
 * it takes a dot product, so the ratified quantity is the unnormalised cross
 * product. Normalising here would add a rounding the reference does not have
 * (qformats §3, one rounding per result). A consumer wanting a unit vector uses
 * §7.4 `normalize3_approx`.
 *
 * Degeneracy is judged on the RESCALED lanes, because that is what
 * `shade_flat_tri` does: it forms nmag2 from the post-rescale fx/fy/fz, so a
 * triangle whose exact cross product is nonzero but rounds to zero is
 * degenerate there too. Reproducing that matters more than being cleverer than
 * it.
 */
inline FaceNormal face_normal(const NormalVertex& a, const NormalVertex& b, const NormalVertex& c,
                              SatLedger* L = nullptr) {
  const int64_t e1x = static_cast<int64_t>(b.x) - a.x;
  const int64_t e1y = static_cast<int64_t>(b.y) - a.y;
  const int64_t e1z = static_cast<int64_t>(b.z) - a.z;
  const int64_t e2x = static_cast<int64_t>(c.x) - a.x;
  const int64_t e2y = static_cast<int64_t>(c.y) - a.y;
  const int64_t e2z = static_cast<int64_t>(c.z) - a.z;

  // Q32.32 lanes. The reference uses int64 here and the widest term a
  // legal fx16 lattice can produce fits; the RTL carries 67 bits because it
  // must not wrap for ANY input word, which is a stricter bar than the
  // reference's, not a different law.
  const int64_t n0 = e1y * e2z - e1z * e2y;
  const int64_t n1 = e1z * e2x - e1x * e2z;
  const int64_t n2 = e1x * e2y - e1y * e2x;

  FaceNormal out;
  out.x = rescale_s32(n0, 16, L);  // -> Q16.16 world-units^2
  out.y = rescale_s32(n1, 16, L);
  out.z = rescale_s32(n2, 16, L);
  out.degenerate = (out.x == 0 && out.y == 0 && out.z == 0);
  return out;
}

}  // namespace terrain
}  // namespace zref
