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

// ---- GEOM.PARAMBUF record layer (ruling R7) --------------------------------
//
//   ProjectedVertex, 24 B     screen_x s32 (legal s21), screen_y s32 (legal
//                             s21), invw24 + status byte, u_over_w s32,
//                             v_over_w s32, rgba8 u32
//   TriangleDescriptor, 16 B  vertex_id[3] u16, material_id u16,
//                             raster_state u32, source_id u32
//   Tile-reference chunk, 64B next_chunk u32, count u16, frame_generation u16,
//                             fourteen triangle ids u32
//
// The two legality rules look like clamps and are NOT. A screen coordinate
// outside s21 and a vertex id past the sealed count are MALFORMED, and
// clamping either would place a triangle somewhere plausible drawn from
// somebody else's data.
inline constexpr int kChunkIds = 14;
inline constexpr uint32_t kChunkNull = 0xFFFFFFFFu;

// s21 fits when every bit above bit 20 equals bit 20. No comparison needed.
inline bool parambuf_fits_s21(int32_t v) {
  const uint32_t hi = static_cast<uint32_t>(v) >> 20;
  return hi == 0x000u || hi == 0xFFFu;
}

inline bool parambuf_vertex_illegal(int32_t x, int32_t y) {
  return !parambuf_fits_s21(x) || !parambuf_fits_s21(y);
}

inline bool parambuf_triangle_illegal(uint16_t v0, uint16_t v1, uint16_t v2,
                                      uint16_t sealed_vertices) {
  return v0 >= sealed_vertices || v1 >= sealed_vertices ||
         v2 >= sealed_vertices;
}

// A chunk from last frame is valid in every other respect -- sane count,
// in-range next pointer, real triangle ids. The generation is the ONLY thing
// that says it is old, which is why it is per chunk and not per arena.
inline bool parambuf_chunk_stale(uint16_t chunk_gen, uint16_t frame_gen) {
  return chunk_gen != frame_gen;
}

inline bool parambuf_chunk_illegal(uint16_t count, uint32_t next,
                                   uint32_t arena_chunks) {
  return count > kChunkIds || (next != kChunkNull && next >= arena_chunks);
}

// Following a stale or malformed chunk is how one bad record becomes a walk
// through arbitrary memory.
inline bool parambuf_chunk_follow(uint16_t chunk_gen, uint16_t frame_gen,
                                  uint16_t count, uint32_t next,
                                  uint32_t arena_chunks) {
  return !parambuf_chunk_stale(chunk_gen, frame_gen) &&
         !parambuf_chunk_illegal(count, next, arena_chunks) &&
         next != kChunkNull;
}

// ===========================================================================
// GEOM.ASSEMBLE -- the index walk that turns a meshlet into triangles
// ===========================================================================
// Written 2026-09-03 from `BORING_3D_FUNDAMENTALS_AUDIT.md` R1, the most basic
// omission the audit found: MESHFETCH emits `index_offset` and
// `triangle_count`, GEOM.VDECODE accepts neither, GEOM.SETUP expects a
// complete triangle, and `tri_ax_i` was driven only from a harness. Nothing
// turned a meshlet's index stream into triangles.
//
// The arithmetic here is one addition. What it owns is the two rules that can
// be silently wrong: which vertex a local index means, and which local indices
// are legal at all.

// The frozen meshlet limits, and they are limits rather than suggestions: a
// `u8` local index cannot address past 255, and 126 triangles x 3 indices is
// 378 bytes.
inline constexpr unsigned kAssembleMaxVertices  = 64;
inline constexpr unsigned kAssembleMaxTriangles = 126;

inline bool assemble_limits_legal(unsigned vertex_count, unsigned triangle_count) {
  return vertex_count >= 1 && vertex_count <= kAssembleMaxVertices &&
         triangle_count <= kAssembleMaxTriangles;
}

// A local index is legal only BELOW the vertex count. The count is a count,
// not a last index -- the same off-by-one that GEOM.PARAMBUF's triangle
// descriptor already refuses.
inline bool assemble_index_legal(unsigned local, unsigned vertex_count) {
  return local < vertex_count;
}

// Local u8 -> global projected-vertex id.
//
// THE ONE ARITHMETIC ACT of this block, and the reason it is not folded into
// the caller: `vertex_offset` is PER VIEW. The same local index resolves to a
// different projected vertex in view 0 and view 1, because projection is per
// view. A single walk emitting into both views gives view 1 the vertices of
// view 0 -- a correct image in one eye and a subtly wrong one in the other,
// which is the hardest class of bug to see and the easiest to write.
inline unsigned assemble_vertex_id(unsigned vertex_offset, unsigned local) {
  return vertex_offset + local;
}

// How many triangles a legal meshlet emits. Zero for an illegal one: the
// meshlet is refused WHOLE, never as a truncated prefix, because a mesh
// missing its tail looks like a modelling error rather than a fault.
inline unsigned assemble_count(unsigned vertex_count, unsigned triangle_count) {
  return assemble_limits_legal(vertex_count, triangle_count) ? triangle_count : 0u;
}

struct AssembledTriangle {
  unsigned v[3];
  bool legal;
};

// Triangle `n` of the walk, in index-stream order.
//
// The order IS the contract: two orderings of the same meshlet produce the
// same picture and different capture CRCs.
//
// A triplet containing an out-of-range local index is REFUSED -- `legal` false
// and no vertex ids to be used. It is not clamped: a clamped index draws a
// triangle from a real vertex belonging to a different part of the mesh, which
// is a visible corruption nothing downstream can detect.
//
// A DEGENERATE triplet -- two or three equal indices -- is legal here and is
// emitted. GEOM.CULL already owns the zero-area decision, and refusing it here
// would put two blocks in charge of one rule.
inline AssembledTriangle assemble_triangle(const unsigned char* indices, unsigned n,
                                           unsigned vertex_offset, unsigned vertex_count) {
  AssembledTriangle t{};
  t.legal = true;
  for (unsigned i = 0; i < 3; ++i) {
    const unsigned local = indices[n * 3 + i];
    if (!assemble_index_legal(local, vertex_count)) {
      t.legal = false;
      t.v[0] = t.v[1] = t.v[2] = 0;
      return t;
    }
    t.v[i] = assemble_vertex_id(vertex_offset, local);
  }
  return t;
}

}  // namespace geom

}  // namespace zref
