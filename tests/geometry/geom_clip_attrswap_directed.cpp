// geom_clip_attrswap_directed.cpp — when the winding flip swaps B and C, do
// their ATTRIBUTES go with them?
//
// ---------------------------------------------------------------------------
// WHY THIS IS ITS OWN FILE
// ---------------------------------------------------------------------------
// GEOM.CLIP implements the double-sided law: a triangle with 2A < 0 has B and C
// swapped so everything downstream sees positive area. Until now it carried no
// attributes, so there was nothing to swap. Ruling 5 gives it an
// attribute-bearing vertex packet, and with it a failure mode that is close to
// undetectable by ordinary means:
//
//   SWAP THE POSITIONS, KEEP THE ATTRIBUTES. Coverage is perfect. Area is
//   positive. Every geometric check passes. The shading is wrong -- but only on
//   the triangles that were wound the other way, which in a scene authored with
//   one convention may be none of them, and in a test written by hand is
//   whichever ones the author happened to type.
//
// So this file drives BOTH windings of the SAME triangle and checks the
// property that actually matters rather than the mechanism:
//
//   interpolating the attribute plane AT A VERTEX must return that vertex's
//   OWN attribute, for all three vertices, after whatever the block did.
//
// That is true for a correct swap and false for a half-swap, and it does not
// care how the block chose to implement either. All seven fields carry
// different values at all three vertices, so swapping only some of them fails
// too.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_geom_clip.h"

#include "zhao_sim.hpp"

namespace {

constexpr int kAttrs = 7;  // invw24, u_over_w, v_over_w, r, g, b, a

int64_t orient(int64_t ux, int64_t uy, int64_t vx, int64_t vy, int64_t px, int64_t py) {
  return (vx - ux) * (py - uy) - (vy - uy) * (px - ux);
}

int64_t div_rhu(__int128 n, int64_t d) {
  const __int128 dd = d;
  return static_cast<int64_t>((n >= 0) ? ((2 * n + dd) / (2 * dd)) : -((-2 * n + dd) / (2 * dd)));
}

struct Vtx {
  int32_t x, y;
  int32_t attr[kAttrs];
};

void put_attr(uint32_t* w, const int32_t* a) {
  for (int i = 0; i < kAttrs; ++i) w[i] = static_cast<uint32_t>(a[i]);
}

void get_attr(const uint32_t* w, int32_t* a) {
  for (int i = 0; i < kAttrs; ++i) a[i] = static_cast<int32_t>(w[i]);
}

struct Out {
  bool accepted;
  int32_t ax, ay, bx, by, cx, cy;
  int64_t area2;
  bool flip;
  int32_t aa[kAttrs], ab[kAttrs], ac[kAttrs];
};

Out run(Vzhao_geom_clip& t, const Vtx& A, const Vtx& B, const Vtx& C) {
  t.rst_n = 0;
  t.tri_valid_i = 0;
  t.out_ready_i = 1;
  t.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();

  t.vp_x0_i = 0;
  t.vp_y0_i = 0;
  t.vp_w_i = 384;
  t.vp_h_i = 240;
  t.cull_mode_i = 0;  // NONE: the double-sided law, which is what flips
  t.tri_ax_i = A.x;
  t.tri_ay_i = A.y;
  t.tri_bx_i = B.x;
  t.tri_by_i = B.y;
  t.tri_cx_i = C.x;
  t.tri_cy_i = C.y;
  t.tri_behind_i = 0;
  t.tri_src_id_i = 0xA5A5;
  put_attr(t.tri_attr_a_i.data(), A.attr);
  put_attr(t.tri_attr_b_i.data(), B.attr);
  put_attr(t.tri_attr_c_i.data(), C.attr);
  t.tri_valid_i = 1;

  for (int i = 0; i < 40; ++i) {
    t.eval();
    if (t.tri_ready_o) {
      zhao::tick(t);
      break;
    }
    zhao::tick(t);
  }
  t.tri_valid_i = 0;

  Out o{};
  for (int i = 0; i < 60; ++i) {
    t.eval();
    if (t.out_valid_o) {
      o.accepted = true;
      o.ax = static_cast<int32_t>(t.out_ax_o);
      o.ay = static_cast<int32_t>(t.out_ay_o);
      o.bx = static_cast<int32_t>(t.out_bx_o);
      o.by = static_cast<int32_t>(t.out_by_o);
      o.cx = static_cast<int32_t>(t.out_cx_o);
      o.cy = static_cast<int32_t>(t.out_cy_o);
      o.area2 = static_cast<int64_t>(t.out_area2_o);
      o.flip = t.out_flip_o != 0;
      get_attr(t.out_attr_a_o.data(), o.aa);
      get_attr(t.out_attr_b_o.data(), o.ab);
      get_attr(t.out_attr_c_o.data(), o.ac);
      zhao::tick(t);
      return o;
    }
    zhao::tick(t);
  }
  return o;
}

/**
 * THE PROPERTY, stated so that ORDER CANNOT ENTER IT.
 *
 * Build the attribute plane the way GEOM.ATTRSETUP does, from the NORMALISED
 * triangle the block emitted, and evaluate it at each of the three SCREEN
 * POSITIONS. At a vertex the two other edge functions are exactly zero and the
 * third is exactly the area, so the result is exactly the attribute sitting in
 * that slot.
 *
 * The expected value is then looked up BY POSITION among the three input
 * vertices -- never by slot. That matters: after a flip, output slot B holds
 * the vertex that arrived in slot C, so comparing slot to slot would report a
 * correct block as broken (it did, on the first run of this file, and the test
 * was what was wrong). Asking "what attribute did the vertex AT THIS POINT ON
 * SCREEN arrive with" is the question the renderer actually cares about, and it
 * is true only if positions and attributes moved together.
 */
bool attrs_follow_vertices(const Out& o, int field, const Vtx& ia, const Vtx& ib, const Vtx& ic) {
  const int64_t area = orient(o.ax, o.ay, o.bx, o.by, o.cx, o.cy);
  if (area <= 0) return false;
  const int64_t va = o.aa[field], vb = o.ab[field], vc = o.ac[field];
  const int64_t verts[3][2] = {{o.ax, o.ay}, {o.bx, o.by}, {o.cx, o.cy}};
  const Vtx* in[3] = {&ia, &ib, &ic};
  for (int k = 0; k < 3; ++k) {
    const int64_t px = verts[k][0], py = verts[k][1];
    // Which INPUT vertex is standing here? The three case vertices are
    // distinct, so this is unambiguous.
    const int32_t* want = nullptr;
    for (int j = 0; j < 3; ++j)
      if (in[j]->x == px && in[j]->y == py) want = in[j]->attr;
    if (want == nullptr) return false;

    const int64_t w0 = orient(o.bx, o.by, o.cx, o.cy, px, py);
    const int64_t w1 = orient(o.cx, o.cy, o.ax, o.ay, px, py);
    const int64_t w2 = orient(o.ax, o.ay, o.bx, o.by, px, py);
    const __int128 num = static_cast<__int128>(w0) * va + static_cast<__int128>(w1) * vb +
                         static_cast<__int128>(w2) * vc;
    if (div_rhu(num, area) != want[field]) return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_geom_clip top;

  auto px = [](int v) { return v * 256; };

  // Seven distinct values at each vertex, all different from each other, so a
  // swap that moves only some fields is caught along with one that moves none.
  Vtx A{px(100), px(100), {11, 12, 13, 14, 15, 16, 17}};
  Vtx B{px(160), px(104), {21, 22, 23, 24, 25, 26, 27}};
  Vtx C{px(120), px(170), {31, 32, 33, 34, 35, 36, 37}};

  // ------------------------------------------------------------------ 1 ---
  printf("== section 1: the counter-clockwise triangle is left alone ==\n");
  {
    const int64_t area = orient(A.x, A.y, B.x, B.y, C.x, C.y);
    zhao::check(area > 0, "the case triangle really is positively wound", 1, area > 0 ? 1 : 0);
    const Out o = run(top, A, B, C);
    zhao::check(o.accepted, "accepted", 1, o.accepted ? 1 : 0);
    zhao::check(!o.flip, "no flip is reported", 0, o.flip ? 1 : 0);
    bool same = o.bx == B.x && o.by == B.y && o.cx == C.x && o.cy == C.y;
    for (int f = 0; f < kAttrs; ++f)
      same = same && o.aa[f] == A.attr[f] && o.ab[f] == B.attr[f] && o.ac[f] == C.attr[f];
    zhao::check(same, "vertices and all seven attribute fields pass through untouched", 1,
                same ? 1 : 0);
  }

  // ------------------------------------------------------------------ 2 ---
  printf("== section 2: the clockwise triangle swaps BOTH, or neither ==\n");
  {
    // The SAME triangle with B and C exchanged: negative area, so the block
    // must flip it back.
    const int64_t area = orient(A.x, A.y, C.x, C.y, B.x, B.y);
    zhao::check(area < 0, "the case triangle really is negatively wound", 1, area < 0 ? 1 : 0);

    const Out o = run(top, A, C, B);  // B and C exchanged at the input
    zhao::check(o.accepted, "accepted", 1, o.accepted ? 1 : 0);
    zhao::check(o.flip, "the flip is reported", 1, o.flip ? 1 : 0);
    zhao::check(o.area2 > 0, "and the emitted area is positive", 1, o.area2 > 0 ? 1 : 0);

    // The positions come back in the original order, because flipping an
    // already-exchanged pair undoes the exchange.
    const bool pos = o.ax == A.x && o.bx == B.x && o.by == B.y && o.cx == C.x && o.cy == C.y;
    zhao::check(pos, "the positions are normalised back to B, C", 1, pos ? 1 : 0);

    // AND THE ATTRIBUTES WITH THEM. This is the check the whole file exists
    // for: it fails on a block that swaps positions and forgets attributes.
    bool attrs = true;
    for (int f = 0; f < kAttrs; ++f)
      attrs = attrs && o.aa[f] == A.attr[f] && o.ab[f] == B.attr[f] && o.ac[f] == C.attr[f];
    zhao::check(attrs, "all seven attribute fields travelled with their vertices", 1,
                attrs ? 1 : 0);
  }

  // ------------------------------------------------------------------ 3 ---
  printf("== section 3: the property, not the mechanism ==\n");
  {
    // Interpolate the plane at each output vertex and require that vertex's own
    // attribute back. True for a correct swap, false for a half-swap, and it
    // makes no assumption about how the block implements either.
    const struct {
      Vtx a, b, c;
      const char* what;
    } cases[] = {
        {A, B, C, "counter-clockwise"},
        {A, C, B, "clockwise"},
    };
    long bad = 0;
    for (const auto& cs : cases) {
      const Out o = run(top, cs.a, cs.b, cs.c);
      if (!o.accepted) {
        ++bad;
        continue;
      }
      for (int f = 0; f < kAttrs; ++f)
        if (!attrs_follow_vertices(o, f, cs.a, cs.b, cs.c)) {
          if (bad < 4)
            printf("      %s: field %d does not interpolate to its vertex\n", cs.what, f);
          ++bad;
        }
    }
    zhao::check(bad == 0, "the attribute at a screen position is the one that vertex arrived with",
                0, (uint32_t)bad);
  }

  // ------------------------------------------------------------------ 4 ---
  printf("== section 4: the two windings really did differ ==\n");
  {
    // ANTI-VACUITY. If both cases happened to be positively wound, section 3
    // would pass without ever exercising a swap. The block's own flip output is
    // the wrong thing to ask -- that is what is under test -- so ask the
    // geometry.
    const int64_t ccw = orient(A.x, A.y, B.x, B.y, C.x, C.y);
    const int64_t cw = orient(A.x, A.y, C.x, C.y, B.x, B.y);
    zhao::check(ccw > 0 && cw < 0, "section 3 drove one of each winding", 1,
                (ccw > 0 && cw < 0) ? 1 : 0);
    bool distinct = true;
    for (int f = 0; f < kAttrs; ++f)
      if (A.attr[f] == B.attr[f] || B.attr[f] == C.attr[f] || A.attr[f] == C.attr[f])
        distinct = false;
    zhao::check(distinct, "and every field differs at every vertex, so a swap is visible", 1,
                distinct ? 1 : 0);
  }

  return zhao::report_and_exit("geom_clip_attrswap_directed");
}
