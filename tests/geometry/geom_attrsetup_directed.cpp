// geom_attrsetup_directed.cpp — the attribute plane against the shipped law.
//
// ---------------------------------------------------------------------------
// WHAT IS ACTUALLY AT RISK
// ---------------------------------------------------------------------------
// `zhao_geom_attrsetup` emits {N0, dNdx, dNdy} — the plane of the numerator in
//
//     attr(x,y) = round_half_up( (w0*va + w1*vb + w2*vc) / area )
//
// and the claim it exists to make is that STEPPING that plane and dividing gives
// the same bits as recomputing the numerator at every pixel, which is what
// reference/src/zrender/rast.cpp does.
//
// So this file does not check three output words against three expected words.
// It reconstructs the attribute AT PIXELS from the block's plane and compares
// against the oracle's own arithmetic, pixel by pixel. A plane that is right at
// the origin and wrong in its gradient would pass the first kind of check and
// fail this one — and a gradient is exactly the field a units mistake corrupts
// while leaving the origin correct.
//
// The 256 is the trap. Screen coordinates carry EIGHT fractional bits, so one
// pixel of x is 256 coordinate units. A gradient computed per coordinate unit
// instead of per pixel is 256x too small, every triangle comes out flat, and
// the value at the origin is still perfect.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_geom_attrsetup.h"

#include "zhao_sim.hpp"

namespace {

// rast.cpp's rounding: round-half-up on the QUOTIENT.
int64_t div_rhu(__int128 n, int64_t d) {
  const __int128 dd = d;
  return static_cast<int64_t>((n >= 0) ? ((2 * n + dd) / (2 * dd)) : -((-2 * n + dd) / (2 * dd)));
}

int64_t orient(int64_t ux, int64_t uy, int64_t vx, int64_t vy, int64_t px, int64_t py) {
  return (vx - ux) * (py - uy) - (vy - uy) * (px - ux);
}

/** Read a signed value out of Verilator's word array for a wide port. */
__int128 wide(const uint32_t* w, int bits) {
  __int128 v = 0;
  const int words = (bits + 31) / 32;
  for (int i = words - 1; i >= 0; --i) v = (v << 32) | w[i];
  // sign-extend from `bits`
  const __int128 one = 1;
  if (v & (one << (bits - 1))) v -= (one << bits);
  return v;
}

struct Tri {
  int32_t ax, ay, bx, by, cx, cy;
  int32_t va, vb, vc;
  const char* what;
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_geom_attrsetup top;

  // px(p) = p * 256: the eight-fractional-bit canvas.
  auto px = [](double p) { return static_cast<int32_t>(p * 256.0); };

  const Tri tris[] = {
      {px(0), px(0), px(64), px(0), px(0), px(48), 0, 1 << 24, -(1 << 24), "right triangle"},
      {px(3), px(5), px(90), px(7), px(11), px(60), 12345, -98765, 4242424, "oblique"},
      {px(-8), px(-4), px(70), px(1), px(2), px(50), 1 << 28, -(1 << 28), 7, "off-canvas apex"},
      // B and C swapped against the obvious ordering, with their attributes,
      // because this one is clockwise otherwise. GEOM.CLIP does that swap for
      // real triangles; the case data has to arrive already normalised.
      {px(10), px(10), px(96), px(11), px(12), px(80), -1, 0, 1, "thin sliver"},
      {px(0), px(0), px(120), px(1), px(1), px(96), 1 << 23, 1 << 23, 1 << 23,
       "constant attribute"},
  };

  long checked = 0;
  for (const Tri& t : tris) {
    const int64_t area = orient(t.ax, t.ay, t.bx, t.by, t.cx, t.cy);
    // GEOM.CLIP hands this block winding-normalised triangles only.
    zhao::check(area > 0, "the case triangle is winding-normalised", 1, area > 0 ? 1 : 0);
    if (area <= 0) continue;

    top.rst_n = 0;
    top.v_valid_i = 0;
    top.r_ready_i = 1;
    top.eval();
    for (int i = 0; i < 2; ++i) zhao::tick(top);
    top.rst_n = 1;
    top.eval();

    top.ax_i = (uint32_t)t.ax;
    top.ay_i = (uint32_t)t.ay;
    top.bx_i = (uint32_t)t.bx;
    top.by_i = (uint32_t)t.by;
    top.cx_i = (uint32_t)t.cx;
    top.cy_i = (uint32_t)t.cy;
    top.va_i = (uint32_t)t.va;
    top.vb_i = (uint32_t)t.vb;
    top.vc_i = (uint32_t)t.vc;
    top.v_valid_i = 1;
    zhao::tick(top);
    top.v_valid_i = 0;
    top.eval();

    char what[96];
    snprintf(what, sizeof what, "%s: the plane is produced", t.what);
    zhao::check(top.r_valid_o == 1, what, 1, (uint32_t)top.r_valid_o);
    if (!top.r_valid_o) continue;

    const __int128 n0 = wide(top.n0_o.data(), 96);
    const __int128 dndx = wide(top.dndx_o.data(), 72);
    const __int128 dndy = wide(top.dndy_o.data(), 72);

    // THE CHECK THAT MATTERS. Reconstruct the attribute at pixel centres from
    // the block's plane, and compare against the oracle recomputing the
    // numerator from scratch at that same pixel.
    long bad = 0;
    for (int64_t py = 0; py < 24; ++py)
      for (int64_t pxl = 0; pxl < 24; ++pxl) {
        const int64_t sx = pxl * 256, sy = py * 256;  // pixel -> canvas units
        const int64_t w0 = orient(t.bx, t.by, t.cx, t.cy, sx, sy);
        const int64_t w1 = orient(t.cx, t.cy, t.ax, t.ay, sx, sy);
        const int64_t w2 = orient(t.ax, t.ay, t.bx, t.by, sx, sy);
        const __int128 num = (__int128)w0 * t.va + (__int128)w1 * t.vb + (__int128)w2 * t.vc;
        const int64_t want = div_rhu(num, area);

        const __int128 stepped = n0 + dndx * pxl + dndy * py;
        const int64_t got = div_rhu(stepped, area);
        if (got != want) {
          if (bad < 3)
            printf("      %s: (%lld,%lld) oracle %lld, plane %lld\n", t.what, (long long)pxl,
                   (long long)py, (long long)want, (long long)got);
          ++bad;
        }
        ++checked;
      }
    snprintf(what, sizeof what, "%s: every pixel matches the oracle's own arithmetic", t.what);
    zhao::check(bad == 0, what, 0, (uint32_t)bad);

    // A CONSTANT ATTRIBUTE HAS A FLAT PLANE. Cheap, and it catches a gradient
    // that is merely SMALL rather than zero -- which a units mistake produces.
    if (t.va == t.vb && t.vb == t.vc) {
      zhao::check(dndx == 0 && dndy == 0,
                  "a constant attribute has exactly zero gradient in both axes", 1,
                  (dndx == 0 && dndy == 0) ? 1 : 0);
    }
  }

  printf("   MEASURED: %ld pixel-attributes reconstructed from the emitted planes\n", checked);
  return zhao::report_and_exit("geom_attrsetup_directed");
}
