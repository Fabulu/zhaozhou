// geom_attrpack_directed.cpp -- does the three-plane front end put the RIGHT
// attribute on the RIGHT plane, three times, from one shared core?
//
// ---------------------------------------------------------------------------
// WHAT IS ACTUALLY AT RISK, AND WHAT IS NOT
// ---------------------------------------------------------------------------
// The plane ARITHMETIC is not at risk here and is not re-checked here.
// `zhao_geom_attrsetup` owns it and `geom_attrsetup_directed.cpp` already
// proves it against `reference/src/zrender/rast.cpp` pixel by pixel. Writing
// the cross products again in this file would be a second implementation of
// the ratified arithmetic, which is the failure CLAUDE.md names by name.
//
// What IS at risk is everything `zhao_geom_attrpack` adds on top:
//
//   * ROUTING. Three lanes share one core through an operand mux. A mux that
//     selects slot 1 for lane 2 produces three perfectly well-formed planes
//     carrying the wrong attributes -- and every handshake, every counter and
//     every width check still passes. A textured surface would simply have its
//     U and V exchanged, which is a picture bug with no alarm.
//   * SEQUENCING. The core is time-multiplexed, so lane k's answer has to be
//     captured into plane k and not into k-1 or k+1. Off by one in either
//     direction still emits 240 x 3 valid-looking bits.
//   * CARRY-OVER. A second triangle must not inherit any part of the first.
//     The block holds nine latched attribute words and three captured planes;
//     a lane that stopped asking would keep the previous triangle's plane and
//     look completely healthy.
//
// ---------------------------------------------------------------------------
// THE PROPERTY, STATED SO THAT ORDER CANNOT ENTER IT
// ---------------------------------------------------------------------------
// Evaluate each emitted plane AT EACH OF THE THREE VERTICES. At a vertex two
// edge functions are exactly zero and the third is exactly the area, so the
// quotient is exactly the attribute sitting in that slot at that vertex.
//
// This is `geom_clip_attrswap_directed.cpp`'s property, reused deliberately:
// it does not care how the block implements the multiplexing, it asks the
// question the renderer actually cares about, and it is FALSE for every one of
// the three risks above. A lane/plane exchange fails it because plane 1
// evaluated at vertex A would return slot 2's value.
//
// The four slots Packet-D does not carry are given values that would be
// unmistakable if they leaked -- 0x5A5A_5A5A and friends -- so "the mux picked
// slot 4" is a loud failure rather than a plausible number.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_geom_attrpack.h"

#include "zhao_sim.hpp"

namespace {

constexpr int kAttrs = 7;   // invw24, u_over_w, v_over_w, r, g, b, a
constexpr int kSlotInvw = 0;
constexpr int kSlotU = 1;
constexpr int kSlotV = 2;

int fails = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    ++fails;
    std::printf("FAIL: %s\n", what);
  }
}

int64_t orient(int64_t ux, int64_t uy, int64_t vx, int64_t vy, int64_t px, int64_t py) {
  return (vx - ux) * (py - uy) - (vy - uy) * (px - ux);
}

// rast.cpp's rounding: round-half-up on the QUOTIENT.
int64_t div_rhu(__int128 n, int64_t d) {
  const __int128 dd = d;
  return static_cast<int64_t>((n >= 0) ? ((2 * n + dd) / (2 * dd)) : -((-2 * n + dd) / (2 * dd)));
}

/** A signed field of `bits` starting at `lo` inside a wide Verilator port. */
__int128 field(const uint32_t* w, int lo, int bits) {
  __int128 v = 0;
  for (int i = bits - 1; i >= 0; --i) {
    const int b = lo + i;
    v = (v << 1) | ((w[b >> 5] >> (b & 31)) & 1u);
  }
  const __int128 one = 1;
  if (v & (one << (bits - 1))) v -= (one << bits);
  return v;
}

struct Plane {
  __int128 n0, dndx, dndy;
};

// {n0[95:0], dndx[71:0], dndy[71:0]}, dndy in the low bits -- the layout
// `zhao_raster_tile_pipe_v2` unpacks and the one `zhao_geom_attrpack` emits.
Plane unpack(const uint32_t* w) {
  Plane p;
  p.dndy = field(w, 0, 72);
  p.dndx = field(w, 72, 72);
  p.n0 = field(w, 144, 96);
  return p;
}

struct Vtx {
  int32_t x, y;                 // S 12.8 canvas coordinates
  int32_t attr[kAttrs];
};

void put_attr(uint32_t* w, const int32_t* a) {
  for (int i = 0; i < kAttrs; ++i) w[i] = static_cast<uint32_t>(a[i]);
}

struct Result {
  bool got;
  Plane plane[3];
  uint32_t src_id;
  uint32_t triangles, planes;
  int clocks;
};

/** Offer one triangle, run until the pack is emitted, accept it. */
Result run_one(Vzhao_geom_attrpack& t, const Vtx& A, const Vtx& B, const Vtx& C,
               uint16_t src_id, int hold_ready_low, bool untex = false) {
  Result r{};
  t.tri_untex_i = untex ? 1 : 0;
  t.tri_ax_i = A.x;
  t.tri_ay_i = A.y;
  t.tri_bx_i = B.x;
  t.tri_by_i = B.y;
  t.tri_cx_i = C.x;
  t.tri_cy_i = C.y;
  put_attr(t.tri_attr_a_i.data(), A.attr);
  put_attr(t.tri_attr_b_i.data(), B.attr);
  put_attr(t.tri_attr_c_i.data(), C.attr);
  t.tri_src_id_i = src_id;
  t.tri_valid_i = 1;
  t.out_ready_i = 0;

  bool accepted = false;
  int held = 0;
  for (int i = 0; i < 200; ++i) {
    t.eval();
    if (!accepted && t.tri_ready_o) {
      accepted = true;
      zhao::tick(t);
      t.tri_valid_i = 0;
      // Nothing must be accepted while the block is working; the triangle is
      // withdrawn so a second accept would be a ready that lied.
      continue;
    }
    if (t.out_valid_o) {
      if (held < hold_ready_low) {
        // Backpressure: the pack must stand still, not advance or decay.
        ++held;
        zhao::tick(t);
        continue;
      }
      r.got = true;
      for (int k = 0; k < 3; ++k) {
        const uint32_t* w = (k == 0)   ? t.out_invw_plane_o.data()
                            : (k == 1) ? t.out_u_over_w_plane_o.data()
                                       : t.out_v_over_w_plane_o.data();
        r.plane[k] = unpack(w);
      }
      r.src_id = t.out_src_id_o;
      r.triangles = t.triangles_o;
      r.planes = t.planes_o;
      r.clocks = i;
      t.out_ready_i = 1;
      zhao::tick(t);
      t.out_ready_i = 0;
      t.eval();
      return r;
    }
    zhao::tick(t);
  }
  return r;
}

/**
 * THE PROPERTY. Evaluate `p` at the pixel the vertex sits on and compare with
 * that vertex's own attribute in `slot`.
 *
 * The vertices are placed on exact pixel centres, so `x >> 8` is exact and the
 * gradient's per-pixel scaling is exercised rather than sidestepped: a plane
 * whose dNdx was built per COORDINATE UNIT instead of per pixel is 256x too
 * small and fails at every vertex but the first.
 */
bool plane_holds_at(const Plane& p, int64_t area, const Vtx& v, int slot) {
  const __int128 n = p.n0 + (__int128)(v.x >> 8) * p.dndx + (__int128)(v.y >> 8) * p.dndy;
  return div_rhu(n, area) == static_cast<int64_t>(v.attr[slot]);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_geom_attrpack top;

  auto px = [](int p) { return static_cast<int32_t>(p * 256); };

  // The four slots Packet-D never carries. If the operand mux reaches one of
  // them the failure is unmistakable rather than plausible.
  const int32_t kLoud[4] = {0x5A5A5A5A, 0x3C3C3C3C, static_cast<int32_t>(0xDEADBEEF),
                            0x0BADF00D};

  auto mk = [&](int xpix, int ypix, int32_t invw, int32_t u, int32_t v) {
    Vtx t{};
    t.x = px(xpix);
    t.y = px(ypix);
    t.attr[kSlotInvw] = invw;
    t.attr[kSlotU] = u;
    t.attr[kSlotV] = v;
    for (int i = 0; i < 4; ++i) t.attr[3 + i] = kLoud[i];
    return t;
  };

  struct Case {
    Vtx a, b, c;
    uint16_t src_id;
    int hold;
    const char* what;
  };

  // Every triangle is counter-clockwise in this coordinate system (area > 0),
  // which is what GEOM.CLIP guarantees its consumers. All three slots differ at
  // all three vertices, so an exchange of any two cannot pass by coincidence.
  const Case cases[] = {
      {mk(0, 0, 0x100000, 1 << 20, -(1 << 20)),
       mk(64, 0, 0x200000, -(1 << 19), 3 << 18),
       mk(0, 48, 0x300000, 7 << 17, 5 << 16), 0x00B2, 0, "right triangle"},
      {mk(3, 5, 0x0F0F0F, 12345, -98765),
       mk(90, 7, 0x010203, -4242424, 777777),
       mk(11, 60, 0x7F0000, 31337, -1), 0x1234, 3, "oblique, held under backpressure"},
      {mk(-8, -4, 0x000001, 1 << 28, -(1 << 28)),
       mk(70, 1, 0xFFFFFF, -(1 << 27), 1 << 26),
       mk(2, 50, 0x800000, 7, -7), 0xFFFF, 0, "off-canvas apex, extreme attributes"},
      // A constant attribute makes both gradients exactly zero. A block that
      // captured the WRONG lane would still show zero gradients here, which is
      // why this case is last and never alone.
      {mk(0, 0, 0x123456, 99, -99),
       mk(120, 1, 0x123456, 99, -99),
       mk(1, 96, 0x123456, 99, -99), 0x0001, 7, "constant attribute, long hold"},
  };

  top.rst_n = 0;
  top.tri_valid_i = 0;
  top.out_ready_i = 0;
  top.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();

  check(top.triangles_o == 0 && top.planes_o == 0, "counters start at zero after reset");

  uint32_t prev_tris = 0, prev_planes = 0;
  int ncase = 0;
  for (const Case& cs : cases) {
    ++ncase;
    const int64_t area = orient(cs.a.x, cs.a.y, cs.b.x, cs.b.y, cs.c.x, cs.c.y);
    if (area <= 0) {
      std::printf("FAIL: case '%s' is wound backwards; GEOM.CLIP would never emit it\n",
                  cs.what);
      ++fails;
      continue;
    }

    const Result r = run_one(top, cs.a, cs.b, cs.c, cs.src_id, cs.hold);
    if (!r.got) {
      std::printf("FAIL: case '%s' never produced a pack\n", cs.what);
      ++fails;
      continue;
    }

    const int slots[3] = {kSlotInvw, kSlotU, kSlotV};
    const Vtx* vs[3] = {&cs.a, &cs.b, &cs.c};
    for (int lane = 0; lane < 3; ++lane) {
      for (int k = 0; k < 3; ++k) {
        char msg[192];
        std::snprintf(msg, sizeof(msg),
                      "case '%s': plane %d evaluated at vertex %c returns that vertex's slot %d",
                      cs.what, lane, "ABC"[k], slots[lane]);
        check(plane_holds_at(r.plane[lane], area, *vs[k], slots[lane]), msg);
      }
    }

    // The loud slots must not appear anywhere in the emitted planes' constant
    // terms. This is a second, cruder net under the property above: it catches
    // a mux that reached slot 3..6 even in the degenerate cases where the
    // property could be satisfied by accident.
    for (int lane = 0; lane < 3; ++lane) {
      for (int i = 0; i < 4; ++i) {
        const __int128 leaked = (__int128)kLoud[i] * (__int128)area;
        char msg[160];
        std::snprintf(msg, sizeof(msg), "case '%s': plane %d did not sample the unused slot %d",
                      cs.what, lane, 3 + i);
        check(r.plane[lane].n0 != leaked, msg);
      }
    }

    char msg[160];
    std::snprintf(msg, sizeof(msg), "case '%s': the identity travelled with the planes", cs.what);
    check(r.src_id == cs.src_id, msg);

    // THE COUNTERS ARE READ AS DELTAS. An absolute compare passes on a block
    // that counts nothing and was reset, which is the reassuring direction.
    std::snprintf(msg, sizeof(msg), "case '%s': exactly one triangle was counted", cs.what);
    check(r.triangles - prev_tris == 1, msg);
    std::snprintf(msg, sizeof(msg),
                  "case '%s': exactly THREE planes were asked for -- one core, three lanes",
                  cs.what);
    check(r.planes - prev_planes == 3, msg);
    prev_tris = r.triangles;
    prev_planes = r.planes;

    // The shared core costs clocks, and the budget argument in the header is
    // "seven against forty-one". A regression that silently tripled the cost
    // would otherwise be invisible until a frame-rate measurement.
    std::snprintf(msg, sizeof(msg), "case '%s': the pack took under 16 clocks plus its hold",
                  cs.what);
    check(r.clocks <= 16 + cs.hold, msg);
  }

  check(top.planes_o == 3 * top.triangles_o,
        "at rest, planes is exactly three times triangles -- no lane stopped asking");
  check(top.triangles_o == static_cast<uint32_t>(ncase), "every case was counted once");

  // -------------------------------------------------------------- case 5 --
  // THE UNTEXTURED DECLARATION (owner ruling R197). This block is the only
  // reader of slots u_over_w and v_over_w in the tree, so it is where "when
  // the bit is set, nothing reads the slot" is either true or not. The same
  // right triangle as case 1, with LOUD values planted in slots 1 and 2 --
  // values that, read, would produce planes whose constant term is
  // unmistakable -- and the bit set:
  //   * the invw24 plane must be exactly what it is without the bit (the
  //     declaration governs u/v only);
  //   * the u/w and v/w planes must be the NULL plane {0, 0, 0}: the slot
  //     content never entered the arithmetic;
  //   * the lane schedule is unchanged: three planes were still asked for;
  //   * and the NEXT triangle, with the bit clear, must read its slots again
  //     (no carry-over of the branch, no carry-over of the null plane).
  {
    ++ncase;
    Vtx a = mk(0, 0, 0x100000, 0x5A5A5A5A, static_cast<int32_t>(0xDEADBEEF));
    Vtx b = mk(64, 0, 0x200000, 0x3C3C3C3C, 0x0BADF00D);
    Vtx c = mk(0, 48, 0x300000, static_cast<int32_t>(0xCAFEBABE), 0x7FFFFFFF);
    const int64_t area = orient(a.x, a.y, b.x, b.y, c.x, c.y);
    const Result r = run_one(top, a, b, c, 0x0197, 2, /*untex=*/true);
    check(r.got, "case 'untex': the pack was emitted");
    if (r.got) {
      for (int k = 0; k < 3; ++k) {
        const Vtx* vs[3] = {&a, &b, &c};
        check(plane_holds_at(r.plane[0], area, *vs[k], kSlotInvw),
              "case 'untex': the invw24 plane is unaffected by the declaration");
      }
      check(r.plane[1].n0 == 0 && r.plane[1].dndx == 0 && r.plane[1].dndy == 0,
            "case 'untex': the u/w plane is the NULL plane -- slot 1 was not read");
      check(r.plane[2].n0 == 0 && r.plane[2].dndx == 0 && r.plane[2].dndy == 0,
            "case 'untex': the v/w plane is the NULL plane -- slot 2 was not read");
      check(r.src_id == 0x0197, "case 'untex': the identity travelled with the planes");
      check(r.triangles - prev_tris == 1 && r.planes - prev_planes == 3,
            "case 'untex': one triangle, three lanes -- the schedule is unchanged");
      prev_tris = r.triangles;
      prev_planes = r.planes;
    }

    // The bit cleared again on the very next triangle: every slot read.
    ++ncase;
    Vtx a2 = mk(0, 0, 0x100000, 1 << 20, -(1 << 20));
    Vtx b2 = mk(64, 0, 0x200000, -(1 << 19), 3 << 18);
    Vtx c2 = mk(0, 48, 0x300000, 7 << 17, 5 << 16);
    const int64_t area2 = orient(a2.x, a2.y, b2.x, b2.y, c2.x, c2.y);
    const Result r2 = run_one(top, a2, b2, c2, 0x0198, 0, /*untex=*/false);
    check(r2.got, "case 'untex cleared': the pack was emitted");
    if (r2.got) {
      const int slots[3] = {kSlotInvw, kSlotU, kSlotV};
      const Vtx* vs[3] = {&a2, &b2, &c2};
      for (int lane = 0; lane < 3; ++lane)
        for (int k = 0; k < 3; ++k)
          check(plane_holds_at(r2.plane[lane], area2, *vs[k], slots[lane]),
                "case 'untex cleared': every plane reads its slot again -- no carry-over");
      prev_tris = r2.triangles;
      prev_planes = r2.planes;
    }
    check(top.planes_o == 3 * top.triangles_o,
          "after the untex cases, planes is still exactly three times triangles");
    check(top.triangles_o == static_cast<uint32_t>(ncase), "the untex cases were counted");
  }

  std::printf("geom_attrpack_directed: %d case(s), counters triangles=%u planes=%u, %d failure(s)\n",
              ncase, top.triangles_o, top.planes_o, fails);

  // NOT `return`. `zhao_sim.hpp` records the reason at `exit_hard`: Verilator
  // 5.051 against winlibs libwinpthread deadlocks in `VlThreadPool::~VlThreadPool()`
  // during exit-time static destruction, and the exe then sits ALIVE AT ~0 CPU
  // with its verdict still in an unflushed buffer. This file was written with a
  // plain `return` and reproduced it exactly: 0.02 CPU seconds after five
  // minutes of wall time, a ctest TIMEOUT, and an empty output file -- while
  // the identical sources built by hand outside CMake ran in under a second,
  // which is the reading that sends you looking at the test logic instead of
  // the toolchain.
  zhao::exit_hard(fails == 0 ? 0 : 1);
}
