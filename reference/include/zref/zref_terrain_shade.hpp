// zref_terrain_shade.hpp — a THIN VIEW onto the ratified flat-shade law.
//
// ---------------------------------------------------------------------------
// THE CORRECTION THIS FILE CARRIES
// ---------------------------------------------------------------------------
// The first version of this header re-implemented the terrain light: its own
// restoring square root, its own divide, s1.15 output, sign preserved. That was
// A SECOND IMPLEMENTATION OF A LAW THAT ALREADY EXISTS, which this repository
// forbids and which its sibling header says out loud:
//
//   zref_terrain_normals.hpp: "This is a THIN view onto an existing ratified
//   law, not a second implementation of it."
//
// The law is `zref::render::shade_flat_tri_dir`
// (`reference/src/zrender/terrain.cpp`), described in
// `reference/src/zrender/internal.hpp` as "The ONE flat-shade law". The golden
// captures pin it. A second arithmetic that merely looks equivalent would mean
// "RTL == oracle" no longer implies "RTL == what the captures already pin",
// which is the only thing that makes an oracle worth having.
//
// So this header states the law's SHAPE for the hardware contract and defines
// only what the ratified function does NOT expose. It does not recompute it.
//
// ---------------------------------------------------------------------------
// WHAT THE RATIFIED LAW ACTUALLY DOES (read from the source, not assumed)
// ---------------------------------------------------------------------------
//   * the face normal fx/fy/fz is Q16.16, one rescale(.,16) per lane -- this
//     is exactly what `zref::terrain::face_normal` returns;
//   * the light is Q16.16, hand-normalised unit (1,2,1)/sqrt(6):
//     kLightX 26758, kLightY 53521, kLightZ 26758. NOT s1.15, which the first
//     version of this header assumed;
//   * ndot is __int128, nmag2 is uint64;
//   * the shade is `div_rhu_s128(ndot, isqrt_u64(nmag2))` -- ONE rounding --
//     giving Q16.16;
//   * and it is CLAMPED to [0, 0x10000], with a zero-area triangle returning 0.
//
// ---------------------------------------------------------------------------
// THE ARCHITECTURAL CONSEQUENCE, WHICH IS THE POINT OF WRITING THIS DOWN
// ---------------------------------------------------------------------------
// The clamp to zero happens INSIDE the ratified function. A detail normal's
// contribution must be added to the lit term BEFORE that clamp -- a face
// slightly turned from the sun has a small negative base, and relief on it
// should still be able to catch light. Adding detail to an already-clamped
// zero cannot darken and cannot brighten from below.
//
// So TERRAIN.SHADE cannot simply call the existing function and hand the result
// on. Either:
//
//   (a) the ratified function grows an UNCLAMPED variant that both it and
//       TERRAIN.NORMALMAP consume, the clamp moving to the single point where
//       the shade becomes a colour; or
//   (b) the detail term is passed INTO it, so one function owns the sum and
//       the clamp.
//
// **(a) is the recommendation** -- it keeps the detail out of a function that
// terrain and creatures share, and it is a pure refactor whose golden CRCs
// must not move. **This is an owner decision and the RTL is blocked on it**,
// which is exactly the sort of thing that should surface before a block is
// built rather than after.
#pragma once

#include <cstdint>

#include "zref/zref_terrain_normals.hpp"

namespace zref {
namespace terrain {

// The renderer's ONE key light, Q16.16, unit (1,2,1)/sqrt(6). Mirrored here
// from `reference/src/zrender/internal.hpp` so the hardware contract can state
// its input format; the VALUES stay owned there and W3.7's look is tuned
// against them, so a change regenerates golden CRCs.
constexpr int32_t kShadeLightX = 26758;
constexpr int32_t kShadeLightY = 53521;
constexpr int32_t kShadeLightZ = 26758;

// Full scale of the ratified shade: Q16.16, so 0x10000 is 1.0. Note this is
// NOT the unit8 convention (raw/256) used elsewhere -- mixing the two is how a
// shade ends up 256x wrong and still looks plausible in a table.
constexpr int32_t kShadeOne = 0x10000;

// ---------------------------------------------------------------------------
// Rounding, shared with TERRAIN.NORMALMAP.
// ---------------------------------------------------------------------------
// qformats §3 is round-half-up, one rounding per result. A shift FLOORS, so
// the two disagree on every negative value -- half of what a detail normal
// produces, and half of what a face turned from the sun produces.
inline int64_t rshift_round(int64_t v, int shift) {
  const int64_t half = int64_t(1) << (shift - 1);
  return (v + half) >> shift;
}

// The squared norm of a Q16.16 face normal, in UNSIGNED 64 -- the same type
// the ratified law uses, and for the same reason. A component at the fx16 rail
// squares to 2^62 and three of those reach 1.38e19 against a signed-64 maximum
// of 9.22e18. `TERRAIN.NORMALS`' contract says the rails are reachable inside
// the domain, so signed accumulation is undefined behaviour in C++ and a
// silent wrap in RTL -- giving a SMALL norm and therefore a huge, wrong shade.
inline uint64_t shade_nmag2(const FaceNormal& n) {
  return static_cast<uint64_t>(static_cast<int64_t>(n.x) * n.x) +
         static_cast<uint64_t>(static_cast<int64_t>(n.y) * n.y) +
         static_cast<uint64_t>(static_cast<int64_t>(n.z) * n.z);
}

// dot(n, L) for a Q16.16 normal and Q16.16 light. Widened to __int128 exactly
// as the ratified law does; the division that follows is the law's and is NOT
// duplicated here.
inline __int128 shade_ndot(const FaceNormal& n, int32_t lx, int32_t ly,
                           int32_t lz) {
  return static_cast<__int128>(n.x) * lx + static_cast<__int128>(n.y) * ly +
         static_cast<__int128>(n.z) * lz;
}

// Is this triangle lightable at all? Zero-area returns 0 from the ratified
// law, and the hardware must agree rather than dividing by zero.
inline bool shade_degenerate(const FaceNormal& n) {
  return shade_nmag2(n) == 0;
}

}  // namespace terrain
}  // namespace zref
