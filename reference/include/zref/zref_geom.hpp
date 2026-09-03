// zref_geom.hpp — the GEOM.CLIP / GEOM.SETUP / GEOM.BINNER reference models
// (phase 5, ZH-056 / ZH-057 / ZH-058).
//
// The three scalar oracles named by design/blocks.yml (`reference_model:
// zref::Clip`, `zref::Setup`, `zref::Binner`) and by their contracts. They are
// the geometry front that feeds RASTER.EDGEWALK: a triangle enters as three
// projected screen vertices and leaves as a stream of (triangle × tile) jobs,
// which is exactly RASTER.EDGEWALK's job port.
//
// ---------------------------------------------------------------------------
// WHAT IS DELEGATED, AND WHAT IS NOT
// ---------------------------------------------------------------------------
// Nothing here restates a law that reference/src/zrender/rast.cpp already owns:
//
//   · the 2A cross product is `zref::EdgeWalk::area2`, which IS rast.cpp's
//     `orient()` — orient(a, b, px, py) == EdgeWalk::area2({a, b, (px,py)}).
//     Clip's zero-area / winding verdict and Setup's plane identity are both
//     checked against that function, not against a re-derivation.
//   · the scan box is `zref::render::scan_bbox` — the SAME function raster_tri
//     itself calls, extracted from it for this increment (internal.hpp). So
//     GEOM.CLIP's viewport test is literally the software raster's own early
//     return, including the 2026-08-15 pixel-centre defect fix.
//   · the fill predicate is the §8 rule proved on `zhao_raster_fill.sv`
//     (tests/formal/raster_edgewalk_top_left.sby); `fill_accept` below is its
//     one-line C++ transcription and nothing else.
//
// What IS defined here, because no spec defines it, is the BINNING law — which
// tiles a triangle is enumerated into and in what order. That is a CHOICE; it
// is argued in fpga/rtl/geometry/zhao_geom_binner.sv and recorded in
// design/contracts/GEOM.BINNER.md as chosen, not found.

#pragma once

#include <cstdint>
#include <vector>

namespace zref {

/**
 * The §8 top-left fill predicate, on the narrow (E', r != 0, top-left) form.
 * This is the C++ transcription of `zhao_raster_fill.sv` — the module the
 * formal lane proves equal to `E0 + bias >= 0` for every (E', r). It is not a
 * second fill rule; it is the same expression, in the other language.
 */
inline bool fill_accept(int64_t ep, bool rnz, bool tl) {
  return (ep >= 0) && (tl || rnz || ep != 0);
}

/** GEOM.CLIP — near-plane rejection, winding, backface, scissor. */
struct Clip {
  /** +/-2048 px guard band in S 12.8 subpixels (spec/qformats.md §8). */
  static constexpr int32_t kGuard = 524288;

  /**
   * Backface culling mode. kCullNone is the DEFAULT and the only mode the
   * software raster has: rast.cpp is double-sided (`area < 0` flips the
   * winding, it never rejects). The other two exist because the ledger's
   * purpose line names "backface cull" while no spec ratifies a winding —
   * see the RTL header and the contract, where the choice is recorded.
   */
  enum CullMode : uint8_t { kCullNone = 0, kCullNegative = 1, kCullPositive = 2 };

  /** Why a triangle left. Order matters: it is the order the tests are made. */
  enum Verdict : uint8_t {
    kAccept = 0,
    kNearPlane = 1,  // some vertex had w <= 0 — whole-primitive rejection
    kZeroArea = 2,   // rast.cpp `if (area == 0) return;`
    kBackface = 3,   // cull_mode rejected the sign of 2A
    kOffscreen = 4   // the scissored scan box is empty
  };

  /** Scissor rectangle in whole pixels (zref::render::Viewport, non-negative). */
  struct Viewport {
    int32_t x0 = 0, y0 = 0, w = 0, h = 0;
  };

  /** One projected triangle. `behind` bit k = vertex k had w <= 0. */
  struct In {
    int32_t ax = 0, ay = 0, bx = 0, by = 0, cx = 0, cy = 0;
    uint8_t behind = 0;
  };

  /** The accepted packet. Every field is meaningless unless `verdict == kAccept`. */
  struct Out {
    Verdict verdict = kAccept;
    int32_t ax = 0, ay = 0, bx = 0, by = 0, cx = 0, cy = 0;  // winding-normalised
    int64_t area2 = 0;                                       // 2A > 0
    int32_t min_x = 0, max_x = 0, min_y = 0, max_y = 0;      // scissored, inclusive
  };

  static Out clip(const In& t, const Viewport& vp, CullMode cull);
};

/**
 * GEOM.SETUP — the affine decomposition of rast.cpp's `orient()`.
 *
 * E_i(px, py) = kx_i * px + ky_i * py + kc_i, in subpixel^2, EXACT, where
 * (px, py) is a subpixel position. Edge 0 = (B,C), 1 = (C,A), 2 = (A,B), the
 * same numbering as rast.cpp's w0 / w1 / w2. The identity
 * `E_i(px,py) == orient(a_i, b_i, px, py)` is asserted against
 * `zref::EdgeWalk::area2` in the directed test.
 */
struct Setup {
  struct Edge {
    int32_t kx = 0;   // -(b.y - a.y): the per-subpixel x coefficient
    int32_t ky = 0;   // +(b.x - a.x): the per-subpixel y coefficient
    int64_t kc = 0;   // a.x*b.y - a.y*b.x
    bool tl = false;  // §8 top-left: (a.y == b.y) ? (a.x < b.x) : (a.y < b.y)
  };

  struct Out {
    Edge e[3] = {};
    int64_t area2 = 0;
  };

  /** Setup for a WINDING-NORMALISED triangle (area2 > 0), as GEOM.CLIP emits. */
  static Out setup(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t cx, int32_t cy,
                   int64_t area2);
};

/**
 * GEOM.BINNER — the tile enumeration.
 *
 * The tile grid is anchored at surface pixel (0,0) with a 16 px pitch (the
 * RASTER.EDGEWALK / RASTER.TILESTORE tile), so tile (tx,ty) owns pixels
 * [16tx, 16tx+16) x [16ty, 16ty+16). Enumeration is ROW-MAJOR over the tiles
 * the scan box touches (ty outer ascending, tx inner ascending), and a tile is
 * emitted only if the three edge functions can still be satisfied somewhere in
 * it — the affine corner test described in the RTL header.
 */
struct Binner {
  static constexpr int kTileLog2 = 4;
  static constexpr int kTile = 1 << kTileLog2;

  struct Ref {
    int32_t tx = 0, ty = 0;
    bool operator==(const Ref& o) const { return tx == o.tx && ty == o.ty; }
    bool operator!=(const Ref& o) const { return !(*this == o); }
  };

  /**
   * The tile references for one accepted, set-up triangle, IN EMISSION ORDER.
   * The scan box is GEOM.CLIP's (inclusive whole pixels, already scissored).
   */
  static std::vector<Ref> bin(const Setup::Out& s, int32_t min_x, int32_t max_x, int32_t min_y,
                              int32_t max_y);

  /**
   * The per-edge screen-wide constants the corner test rides on:
   * E0 at the centre of pixel (0,0), its low 8 bits, and E' there.
   * `ep_at(px,py) = ep_base + kx*px + ky*py` is EXACT (both edge steps are
   * multiples of 256, so `r` is constant over every pixel centre).
   */
  static int64_t e0_base(const Setup::Edge& e) {
    return static_cast<int64_t>(e.kx) * 128 + static_cast<int64_t>(e.ky) * 128 + e.kc;
  }
  static bool rnz(const Setup::Edge& e) { return (e0_base(e) & 255) != 0; }
  static int64_t ep_base(const Setup::Edge& e) { return e0_base(e) >> 8; }
};

// ---- GEOM.VDECODE format 0 — RAW / CANONICAL (ruling R11) -----------------
//
// 32 bytes per vertex, naturally aligned, no bit-packing:
//
//   off  0  12  position s32 x3, fx16
//   off 12   3  normal s8 x3, the packed bind normal the cel path uses
//   off 15   1  w0 in 1/64 quanta (64 = rigid)
//   off 16   4  UV, 2 x s16 fx16
//   off 20   2  bone0
//   off 22   2  bone1
//   off 24   8  reserved, MUST BE ZERO
//
// Format 0 is NOT a placeholder. It is the permanent fallback and the
// differential reference: every later format must decode to bit-identical
// output for the same source mesh, and this is what "the same" is measured
// against. The reserved eight bytes are what formats 1 and 2 grow into without
// changing the stride, and requiring them zero is what stops an older decoder
// silently reading a newer file.
//
// This is the ORACLE for `zhao_geom_vdecode`.
namespace geom {

struct Vertex0 {
  int32_t x, y, z;      // fx16 S15.16
  int8_t nx, ny, nz;    // packed bind normal
  uint8_t w0;           // 1/64 quanta, 64 == rigid
  int16_t u, v;         // fx16 s16
  uint16_t bone0, bone1;
  bool rigid;           // bone1 == bone0
  bool reserved_nonzero;  // the malformed case, reported not corrected
};

inline Vertex0 vdecode0(const uint8_t* b) {
  auto s32 = [&](int o) {
    return static_cast<int32_t>(static_cast<uint32_t>(b[o]) |
                                (static_cast<uint32_t>(b[o + 1]) << 8) |
                                (static_cast<uint32_t>(b[o + 2]) << 16) |
                                (static_cast<uint32_t>(b[o + 3]) << 24));
  };
  auto u16 = [&](int o) {
    return static_cast<uint16_t>(static_cast<uint16_t>(b[o]) |
                                 (static_cast<uint16_t>(b[o + 1]) << 8));
  };
  Vertex0 v{};
  v.x = s32(0);
  v.y = s32(4);
  v.z = s32(8);
  v.nx = static_cast<int8_t>(b[12]);
  v.ny = static_cast<int8_t>(b[13]);
  v.nz = static_cast<int8_t>(b[14]);
  v.w0 = b[15];
  v.u = static_cast<int16_t>(u16(16));
  v.v = static_cast<int16_t>(u16(18));
  v.bone0 = u16(20);
  v.bone1 = u16(22);
  v.rigid = (v.bone1 == v.bone0);
  v.reserved_nonzero = false;
  for (int i = 24; i < 32; ++i)
    if (b[i] != 0) v.reserved_nonzero = true;
  return v;
}

// `w0` is legal in [0, 64]. 64 means rigid; anything above it is a malformed
// asset rather than a saturating weight, and saying so is the difference
// between a bad file and a silently wrong skin.
inline bool vdecode0_w0_legal(uint8_t w0) { return w0 <= 64; }

}  // namespace geom

}  // namespace zref
