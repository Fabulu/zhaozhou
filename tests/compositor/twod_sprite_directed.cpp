// twod_sprite_directed.cpp — does the sprite walker emit the right pixels at
// the right UVs, and does the affine step exactly?
//
// ---------------------------------------------------------------------------
// THE PROPERTY THAT IS EASY TO GET NEARLY RIGHT
// ---------------------------------------------------------------------------
// UV at pixel (px, py) is a plane:
//
//     u = u0 + a00*px + a01*py
//
// Stepping it with adds is exact. Stepping it the SERPENTINE way — carrying
// the running coordinate from the last pixel of one row into the first pixel
// of the next — is also "just adds", also looks right on a 1×N or N×1 sprite,
// and is wrong for every sprite wider than one pixel. Row N's coordinate would
// depend on the width of row N-1.
//
// So the check is not "the UVs advance". It is every pixel of a rectangular
// sprite against the closed form, with a NON-ZERO CROSS TERM — a01 and a10
// zero is exactly the case where the serpentine bug disappears.
//
// ---------------------------------------------------------------------------
// AND THE DELETIONS
// ---------------------------------------------------------------------------
// The ruling for this block is mostly a list of things that do not exist: no
// private HUD sampler, no text rasterizer, no glyph cache. Those cannot be
// tested for absence directly, but their consequence can: this block emits
// SAMPLE REQUESTS and owns no texture memory, so what comes out is coordinates
// and never colour.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_twod_sprite.h"

#include "zhao_sim.hpp"
#include "zref/zref_twod.hpp"

namespace {

struct Px {
  int x, y;
  int32_t u, v;
  int last;
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_twod_sprite top;

  auto reset = [&]() {
    top.d_valid_i = 0;
    top.s_ready_i = 1;
    top.view_sel_i = 1;
    top.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  // Offer a descriptor and collect every sample request it produces.
  auto run = [&](int x, int y, int w, int h, int32_t u0, int32_t v0,
                 int32_t a00, int32_t a01, int32_t a10, int32_t a11,
                 int view_mask, int view_sel, bool stall) {
    std::vector<Px> out;
    top.view_sel_i = view_sel;
    top.d_valid_i = 1;
    top.d_x_i = x; top.d_y_i = y; top.d_w_i = w; top.d_h_i = h;
    top.d_u_i = u0; top.d_v_i = v0;
    top.d_a00_i = a00; top.d_a01_i = a01;
    top.d_a10_i = a10; top.d_a11_i = a11;
    top.d_format_i = 1; top.d_palette_i = 7; top.d_tint_i = 0x1234;
    top.d_blend_i = 2; top.d_view_mask_i = view_mask; top.d_order_i = 9;
    top.d_src_id_i = 0x55AA;
    top.eval();
    zhao::tick(top);
    top.d_valid_i = 0;

    uint32_t g = 0x1234u;
    for (int c = 0; c < 20000; ++c) {
      // A consumer that is not always ready is the normal case, and a walker
      // that only steps correctly when nothing stalls is not finished.
      g = g * 1664525u + 1013904223u;
      top.s_ready_i = stall ? (((g >> 27) & 3u) != 0u) : 1;
      top.eval();
      if (top.s_valid_o && top.s_ready_i) {
        out.push_back({static_cast<int16_t>(top.s_x_o),
                       static_cast<int16_t>(top.s_y_o),
                       static_cast<int32_t>(top.s_u_o),
                       static_cast<int32_t>(top.s_v_o),
                       static_cast<int>(top.s_last_o)});
      }
      const bool done = !top.s_valid_o;
      zhao::tick(top);
      if (done && !out.empty()) break;
    }
    top.s_ready_i = 1;
    return out;
  };

  reset();

  // ---- 1: every pixel, against the CLOSED FORM, with cross terms ---------
  {
    const int W = 7, H = 5;
    const int32_t u0 = 1000, v0 = -2000;
    const int32_t a00 = 17, a01 = -5;     // du/dx, du/dy  -- NON-ZERO CROSS
    const int32_t a10 = 3,  a11 = 29;     // dv/dx, dv/dy
    const auto out = run(100, 50, W, H, u0, v0, a00, a01, a10, a11, 1, 1, false);

    zhao::check(out.size() == static_cast<size_t>(W * H),
                "a 7x5 sprite emits exactly 35 sample requests", W * H,
                static_cast<int>(out.size()));

    int bad_uv = 0, bad_xy = 0, bad_order = 0;
    for (int i = 0; i < static_cast<int>(out.size()); ++i) {
      const int py = i / W, px = i % W;
      // the closed form, which is what the stepping must equal exactly
      const int32_t wu = zref::twod::sprite_u(u0, a00, a01, px, py);
      const int32_t wv = zref::twod::sprite_v(v0, a10, a11, px, py);
      if (out[i].u != wu || out[i].v != wv) ++bad_uv;
      if (out[i].x != 100 + px || out[i].y != 50 + py) ++bad_xy;
      if (out[i].last != (i == W * H - 1 ? 1 : 0)) ++bad_order;
    }
    zhao::check(bad_uv == 0,
                "and every UV equals the closed form u0 + a00*px + a01*py -- "
                "with a NON-ZERO cross term, which is the only case where "
                "serpentine stepping differs from plane stepping",
                0, bad_uv);
    zhao::check(bad_xy == 0, "and every screen position is row-major from the "
                             "descriptor's origin", 0, bad_xy);
    zhao::check(bad_order == 0,
                "and `last` marks exactly the final pixel, once", 0, bad_order);
  }

  // ---- 2: the same, WITH a stalling consumer ----------------------------
  {
    const int W = 6, H = 4;
    const int32_t u0 = -77, v0 = 88;
    const int32_t a00 = 11, a01 = 13, a10 = -7, a11 = 5;
    const auto out = run(0, 0, W, H, u0, v0, a00, a01, a10, a11, 1, 1, true);
    int bad = 0;
    for (int i = 0; i < static_cast<int>(out.size()); ++i) {
      const int py = i / W, px = i % W;
      if (out[i].u != zref::twod::sprite_u(u0, a00, a01, px, py)) ++bad;
      if (out[i].v != zref::twod::sprite_v(v0, a10, a11, px, py)) ++bad;
    }
    zhao::check(out.size() == static_cast<size_t>(W * H) && bad == 0,
                "a stalling consumer changes nothing -- the walk is driven by "
                "the handshake, not by the clock",
                0, bad + static_cast<int>(out.size() != W * H));
  }

  // ---- 3: the VIEW MASK, which is all the two HUD regions are here -------
  {
    const uint32_t skipped_before = top.skipped_view_o;
    const auto out = run(0, 0, 4, 4, 0, 0, 1, 0, 0, 1, /*mask=*/2,
                         /*view=*/1, false);
    zhao::check(out.empty(),
                "a descriptor whose view_mask excludes this view emits NOTHING "
                "-- the two player HUD regions are a compositing concern, and "
                "here they are a mask and nothing more",
                0, static_cast<int>(out.size()));
    zhao::check(top.skipped_view_o == skipped_before + 1,
                "and it is counted as SKIPPED, not refused: one is normal and "
                "one is a bug in the caller",
                1, static_cast<int>(top.skipped_view_o - skipped_before));
  }

  // ---- 4: a degenerate descriptor is REFUSED, not walked for zero pixels --
  {
    const uint32_t refused_before = top.refused_o;
    const auto a = run(0, 0, 0, 4, 0, 0, 1, 0, 0, 1, 1, 1, false);
    const auto b = run(0, 0, 4, 0, 0, 0, 1, 0, 0, 1, 1, 1, false);
    zhao::check(a.empty() && b.empty(), "a zero-width or zero-height sprite "
                                        "emits nothing", 0,
                static_cast<int>(a.size() + b.size()));
    zhao::check(top.refused_o == refused_before + 2,
                "and is REFUSED and counted -- walking it for zero pixels would "
                "be indistinguishable downstream from a sprite that had nothing "
                "to draw",
                2, static_cast<int>(top.refused_o - refused_before));
  }

  // ---- 5: a single pixel, which is where an off-by-one lives -------------
  {
    const auto out = run(3, 4, 1, 1, 42, 43, 100, 200, 300, 400, 1, 1, false);
    zhao::check(out.size() == 1 && out[0].u == 42 && out[0].v == 43 &&
                    out[0].x == 3 && out[0].y == 4 && out[0].last == 1,
                "a 1x1 sprite emits exactly one pixel, at the descriptor's own "
                "UV, marked last",
                1,
                (out.size() == 1 && out[0].u == 42 && out[0].last == 1) ? 1 : 0);
  }

  std::printf("  %u descriptors, %u pixels, %u skipped for view, %u refused\n",
              top.descriptors_o, top.pixels_o, top.skipped_view_o,
              top.refused_o);

  return zhao::report_and_exit("twod_sprite_directed");
}
