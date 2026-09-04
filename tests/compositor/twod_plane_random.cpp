// twod_plane_random.cpp — many planes, many pixels, against the closed form.
//
// ---------------------------------------------------------------------------
// WHAT RANDOM BUYS HERE
// ---------------------------------------------------------------------------
// The directed lane checks each restriction once. What it cannot cover is the
// COORDINATE SPACE: negative coordinates, coordinates far past the plane's
// width, wrap and clamp on both axes at once, and the interaction between line
// scroll and a sheared affine.
//
// Negative is where wrapping goes wrong. `r += size` is correct for a
// coordinate one size below zero and wrong for one that is two sizes below,
// and the difference does not appear until a plane scrolls left past its own
// origin — which every scrolling sky does, once.
//
// So the coefficients and the scroll are drawn wide enough to go negative
// often, both axes are exercised in repeat and in clamp, and the coverage
// guards at the end require that the run actually reached those cases.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_twod_plane.h"

#include "zhao_sim.hpp"
#include "zref/zref_twod.hpp"

namespace {

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_twod_plane top;

  top.d_valid_i = 0;
  top.p_valid_i = 0;
  top.s_ready_i = 1;
  top.view_sel_i = 1;
  top.rst_n = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  uint32_t s = 0x71A9E3u;

  int bad_u = 0, bad_v = 0, bad_valid = 0;
  int pixels = 0, negatives = 0, clamps = 0, wrapfails = 0;

  for (int plane = 0; plane < 120; ++plane) {
    const int slot = static_cast<int>(rnd(&s) & 1u);
    const int role = static_cast<int>(rnd(&s) & 1u);  // legal roles only
    const int blend = (role == 0) ? 0 : static_cast<int>(1 + (rnd(&s) & 1u));
    const int fmt = static_cast<int>(rnd(&s) & 1u);
    const uint16_t w = static_cast<uint16_t>(16 + (rnd(&s) % 500u));
    const uint16_t h = static_cast<uint16_t>(16 + (rnd(&s) % 500u));
    const bool clamp_u = (rnd(&s) & 1u) != 0u;
    const bool clamp_v = (rnd(&s) & 1u) != 0u;
    if (clamp_u || clamp_v) ++clamps;

    // Coefficients small enough that one correction is legitimate, and offsets
    // wide enough to go negative often.
    const int32_t a = static_cast<int32_t>(rnd(&s) % 131072u) - 65536;
    const int32_t b = static_cast<int32_t>(rnd(&s) % 131072u) - 65536;
    const int32_t c = static_cast<int32_t>(rnd(&s) % 131072u) - 65536;
    const int32_t d = static_cast<int32_t>(rnd(&s) % 131072u) - 65536;
    const int32_t u0 = static_cast<int32_t>(rnd(&s) % 4000000u) - 2000000;
    const int32_t v0 = static_cast<int32_t>(rnd(&s) % 4000000u) - 2000000;

    top.d_valid_i = 1;
    top.d_slot_i = slot;
    top.d_role_i = role;
    top.d_blend_i = blend;
    top.d_opacity_i = 200;
    top.d_format_i = fmt;
    top.d_width_i = w;
    top.d_height_i = h;
    top.d_wrap_u_i = clamp_u ? 1 : 0;
    top.d_wrap_v_i = clamp_v ? 1 : 0;
    top.d_a_i = a;
    top.d_b_i = b;
    top.d_c_i = c;
    top.d_d_i = d;
    top.d_u0_i = u0;
    top.d_v0_i = v0;
    top.d_view_mask_i = 3;
    top.d_palette_i = 1;
    zhao::tick(top);
    top.d_valid_i = 0;

    for (int n = 0; n < 40; ++n) {
      const int x = static_cast<int>(rnd(&s) % 640u);
      const int y = static_cast<int>(rnd(&s) % 400u);
      const int32_t scroll = static_cast<int32_t>(rnd(&s) % 2000000u) - 1000000;

      top.p_valid_i = 1;
      top.p_slot_i = slot;
      top.p_x_i = x;
      top.p_y_i = y;
      top.p_line_scroll_i = scroll;
      top.view_sel_i = 1;
      top.s_ready_i = 1;
      top.eval();
      zhao::tick(top);
      top.p_valid_i = 0;
      top.eval();

      const int32_t ui = zref::twod::plane_u(u0, a, b, scroll, x, y);
      const int32_t vi = zref::twod::plane_v(v0, c, d, x, y);
      if (ui < 0 || vi < 0) ++negatives;
      bool fu = false, fv = false;
      const uint16_t wu = zref::twod::plane_wrap(ui, w, clamp_u, &fu);
      const uint16_t wv = zref::twod::plane_wrap(vi, h, clamp_v, &fv);
      if (fu || fv) ++wrapfails;

      if (!top.s_valid_o) {
        ++bad_valid;
        continue;
      }
      if (top.s_texel_u_o != wu) ++bad_u;
      if (top.s_texel_v_o != wv) ++bad_v;
      ++pixels;
    }
  }

  zhao::check(bad_valid == 0,
              "every pixel of an enabled, in-view plane produces a sample "
              "request",
              0, bad_valid);
  zhao::check(bad_u == 0 && bad_v == 0,
              "and every texel matches zref::twod::plane_u/v through "
              "plane_wrap -- affine, line scroll, repeat and clamp together",
              0, bad_u + bad_v);

  // The run has to have REACHED the cases the wrap is hard for. Negative
  // coordinates are where `r += size` is right for one size out and wrong for
  // two, and that does not appear until a plane scrolls past its own origin.
  zhao::check(negatives > 200,
              "the run produced many NEGATIVE coordinates -- where wrapping is "
              "right for one size out and wrong for two",
              1, negatives > 200 ? 1 : 0);
  zhao::check(clamps > 30, "and exercised clamp as well as repeat", 1, clamps > 30 ? 1 : 0);

  std::printf(
      "  %d pixels across 120 planes, %d negative coordinates, "
      "%d clamped planes, %d wrap failures (hw %u)\n",
      pixels, negatives, clamps, wrapfails, top.wrap_fail_o);

  return zhao::report_and_exit("twod_plane_random");
}
