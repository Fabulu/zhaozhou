// twod_sprite_random.cpp — many sprites, many shapes, against the closed form.
//
// ---------------------------------------------------------------------------
// WHAT RANDOM BUYS HERE
// ---------------------------------------------------------------------------
// The directed lane walks a 7x5 and a 6x4 with chosen coefficients. What it
// cannot cover is SHAPE: a 1-pixel-wide sprite, a 1-pixel-tall one, a very
// wide short one, a very tall narrow one. The affine stepper's failure modes
// live at exactly those shapes — a serpentine accumulation is invisible on a
// 1xN, and a row-origin that steps once too many is invisible on an Nx1.
//
// So the shapes are drawn from a distribution that deliberately includes the
// degenerate-adjacent ones, and every pixel is checked against
// `zref::twod::sprite_u/v`.
//
// The consumer stalls at random throughout. A walker driven by the clock
// rather than by the handshake passes every un-stalled test ever written.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_twod_sprite.h"

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
  Vzhao_twod_sprite top;

  top.d_valid_i = 0;
  top.s_ready_i = 1;
  top.view_sel_i = 1;
  top.rst_n = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  uint32_t s = 0x5A17E2u;

  int bad_uv = 0, bad_count = 0, bad_last = 0;
  int sprites = 0, thin = 0, wide = 0, pixels = 0;

  for (int n = 0; n < 300; ++n) {
    // Shapes that include the degenerate-adjacent ones on purpose.
    int w, h;
    switch (rnd(&s) % 5u) {
      case 0: w = 1; h = 1 + static_cast<int>(rnd(&s) % 12u); break;  // 1 wide
      case 1: w = 1 + static_cast<int>(rnd(&s) % 12u); h = 1; break;  // 1 tall
      case 2: w = 1 + static_cast<int>(rnd(&s) % 20u); h = 1 + static_cast<int>(rnd(&s) % 3u); break;
      case 3: w = 1 + static_cast<int>(rnd(&s) % 3u); h = 1 + static_cast<int>(rnd(&s) % 20u); break;
      default: w = 1 + static_cast<int>(rnd(&s) % 9u); h = 1 + static_cast<int>(rnd(&s) % 9u);
    }
    if (w == 1 || h == 1) ++thin;
    if (w >= 12 || h >= 12) ++wide;

    const int32_t u0 = static_cast<int32_t>(rnd(&s) % 40000u) - 20000;
    const int32_t v0 = static_cast<int32_t>(rnd(&s) % 40000u) - 20000;
    const int32_t a00 = static_cast<int32_t>(rnd(&s) % 400u) - 200;
    const int32_t a01 = static_cast<int32_t>(rnd(&s) % 400u) - 200;
    const int32_t a10 = static_cast<int32_t>(rnd(&s) % 400u) - 200;
    const int32_t a11 = static_cast<int32_t>(rnd(&s) % 400u) - 200;
    const int x = static_cast<int>(rnd(&s) % 600u);
    const int y = static_cast<int>(rnd(&s) % 400u);

    top.d_valid_i = 1;
    top.d_x_i = x; top.d_y_i = y; top.d_w_i = w; top.d_h_i = h;
    top.d_u_i = u0; top.d_v_i = v0;
    top.d_a00_i = a00; top.d_a01_i = a01;
    top.d_a10_i = a10; top.d_a11_i = a11;
    top.d_format_i = 1; top.d_palette_i = 0; top.d_tint_i = 0;
    top.d_blend_i = 0; top.d_view_mask_i = 1; top.d_order_i = 0;
    top.d_src_id_i = static_cast<uint16_t>(n);
    top.eval();
    zhao::tick(top);
    top.d_valid_i = 0;

    int got = 0, lasts = 0;
    for (int c = 0; c < 20000; ++c) {
      top.s_ready_i = ((rnd(&s) >> 3) & 3u) != 0u;   // a stalling consumer
      top.eval();
      if (top.s_valid_o && top.s_ready_i) {
        const int py = got / w, px = got % w;
        if (static_cast<int32_t>(top.s_u_o) !=
                zref::twod::sprite_u(u0, a00, a01, px, py) ||
            static_cast<int32_t>(top.s_v_o) !=
                zref::twod::sprite_v(v0, a10, a11, px, py))
          ++bad_uv;
        if (top.s_last_o) ++lasts;
        ++got;
        ++pixels;
      }
      const bool done = !top.s_valid_o;
      zhao::tick(top);
      if (done && got > 0) break;
    }
    top.s_ready_i = 1;

    if (got != w * h) ++bad_count;
    if (lasts != 1) ++bad_last;
    ++sprites;
  }

  zhao::check(bad_uv == 0,
              "every pixel of 300 sprites equals zref::twod::sprite_u/v -- the "
              "closed form, so a serpentine accumulation cannot hide",
              0, bad_uv);
  zhao::check(bad_count == 0, "and each sprite emits exactly w*h pixels", 0,
              bad_count);
  zhao::check(bad_last == 0, "and raises `last` exactly once", 0, bad_last);

  // The shapes have to have INCLUDED the ones the stepper fails on. A run of
  // square sprites would agree with the model and say nothing about either
  // failure mode.
  zhao::check(thin > 50,
              "the run included many 1-pixel-wide or 1-pixel-tall sprites -- "
              "the shapes where a serpentine walk and a row-origin off-by-one "
              "are each invisible",
              1, thin > 50 ? 1 : 0);
  zhao::check(wide > 20, "and many long ones, where they are not", 1,
              wide > 20 ? 1 : 0);

  std::printf("  %d sprites, %d pixels, %d thin, %d long\n", sprites, pixels,
              thin, wide);

  return zhao::report_and_exit("twod_sprite_random");
}
