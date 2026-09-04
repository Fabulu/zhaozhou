// twod_plane_directed.cpp — the restrictions, checked as restrictions.
//
// ---------------------------------------------------------------------------
// MOST OF THIS BLOCK'S CONTRACT IS A LIST OF THINGS IT MAY NOT DO
// ---------------------------------------------------------------------------
// "Two plane descriptors are a real v1 limit, not a placeholder." "Nearest
// sampling only — bilinear is what would make this a second TMU." "No
// arbitrary depth test and no depth write in v1." "Roles 2 and 3 are
// reserved; the descriptor is refused."
//
// A restriction is not tested by exercising the allowed path. It is tested by
// asking for the forbidden one and requiring a refusal — and, where the
// restriction is structural, by noting that there is no port with which to
// ask. Both appear below, labelled, because the second kind is easy to mistake
// for an untested claim.
//
// The one that is neither is WRAPPING. Repeat by one conditional correction is
// exact while the per-pixel step is no larger than the plane, and that is a
// real assumption about callers rather than a law. So the failure is counted,
// and the test drives a caller that violates it on purpose to prove the
// counter moves.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_twod_plane.h"

#include "zhao_sim.hpp"
#include "zref/zref_twod.hpp"

namespace {

constexpr int32_t fx(double v) { return static_cast<int32_t>(v * 65536.0); }

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_twod_plane top;

  auto reset = [&]() {
    top.d_valid_i = 0;
    top.p_valid_i = 0;
    top.s_ready_i = 1;
    top.view_sel_i = 1;
    top.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  auto program = [&](int slot, int role, int blend, int fmt, int w, int h, int wrap_u, int wrap_v,
                     int32_t a, int32_t b, int32_t c, int32_t d, int32_t u0, int32_t v0, int mask) {
    top.d_valid_i = 1;
    top.d_slot_i = slot;
    top.d_role_i = role;
    top.d_blend_i = blend;
    top.d_opacity_i = 128;
    top.d_format_i = fmt;
    top.d_width_i = w;
    top.d_height_i = h;
    top.d_wrap_u_i = wrap_u;
    top.d_wrap_v_i = wrap_v;
    top.d_a_i = a;
    top.d_b_i = b;
    top.d_c_i = c;
    top.d_d_i = d;
    top.d_u0_i = u0;
    top.d_v0_i = v0;
    top.d_view_mask_i = mask;
    top.d_palette_i = 5;
    zhao::tick(top);
    top.d_valid_i = 0;
  };

  struct S {
    bool valid;
    int u, v, role, blend;
  };

  auto pixel = [&](int slot, int x, int y, int32_t scroll, int view) {
    top.view_sel_i = view;
    top.p_valid_i = 1;
    top.p_slot_i = slot;
    top.p_x_i = x;
    top.p_y_i = y;
    top.p_line_scroll_i = scroll;
    top.s_ready_i = 1;
    top.eval();
    zhao::tick(top);
    top.p_valid_i = 0;
    top.eval();
    return S{top.s_valid_o != 0, static_cast<int>(top.s_texel_u_o),
             static_cast<int>(top.s_texel_v_o), static_cast<int>(top.s_role_o),
             static_cast<int>(top.s_blend_o)};
  };

  reset();

  // ---- 1: the affine, against the closed form ---------------------------
  {
    // identity-ish: u = x, v = y, on a 256x128 repeating plane
    program(0, /*BACKDROP*/ 0, /*REPLACE*/ 0, /*RGB565*/ 1, 256, 128, 0, 0, fx(1.0), 0, 0, fx(1.0),
            0, 0, 3);
    int bad = 0;
    for (int y = 0; y < 20; ++y)
      for (int x = 0; x < 20; ++x) {
        const S s = pixel(0, x, y, 0, 1);
        if (!s.valid || s.u != x || s.v != y) ++bad;
      }
    zhao::check(bad == 0, "an identity plane samples texel (x, y)", 0, bad);

    // a scaled and sheared one
    program(0, 0, 0, 1, 256, 128, 0, 0, fx(2.0), fx(0.5), fx(0.25), fx(3.0), fx(10.0), fx(20.0), 3);
    int bad2 = 0;
    for (int y = 0; y < 10; ++y)
      for (int x = 0; x < 10; ++x) {
        const S s = pixel(0, x, y, 0, 1);
        bool fu = false, fv = false;
        const int wu = zref::twod::plane_wrap(
            zref::twod::plane_u(fx(10.0), fx(2.0), fx(0.5), 0, x, y), 256, false, &fu);
        const int wv = zref::twod::plane_wrap(
            zref::twod::plane_v(fx(20.0), fx(0.25), fx(3.0), x, y), 128, false, &fv);
        if (!s.valid || s.u != wu || s.v != wv) ++bad2;
      }
    zhao::check(bad2 == 0,
                "and a scaled, sheared one samples the affine's own texel, "
                "wrapped",
                0, bad2);
  }

  // ---- 2: LINE SCROLL adds to u, and only to u --------------------------
  {
    program(0, 0, 0, 1, 256, 128, 0, 0, fx(1.0), 0, 0, fx(1.0), 0, 0, 3);
    const S a = pixel(0, 4, 6, 0, 1);
    const S b = pixel(0, 4, 6, fx(40.0), 1);
    zhao::check(a.u == 4 && b.u == 44 && a.v == b.v, "line scroll shifts U and leaves V alone", 1,
                (a.u == 4 && b.u == 44 && a.v == b.v) ? 1 : 0);
  }

  // ---- 3: REPEAT and CLAMP --------------------------------------------
  {
    // repeat: u = -1 on a 256-wide plane wraps to 255
    program(0, 0, 0, 1, 256, 128, /*repeat*/ 0, 0, fx(1.0), 0, 0, fx(1.0), fx(-1.0), 0, 3);
    const S r = pixel(0, 0, 0, 0, 1);
    // clamp: the same coordinate clamps to 0
    program(1, 1, 1, 1, 256, 128, /*clamp*/ 1, 1, fx(1.0), 0, 0, fx(1.0), fx(-1.0), 0, 3);
    const S c = pixel(1, 0, 0, 0, 1);
    zhao::check(r.u == 255, "REPEAT wraps -1 to size-1", 255, r.u);
    zhao::check(c.u == 0, "and CLAMP pins it to 0", 0, c.u);
  }

  // ---- 4: TWO SLOTS, ONE ENGINE ---------------------------------------
  // Both slots hold independent descriptors and are selected per pixel. The
  // "one engine" half of the ruling is structural -- there is one set of
  // steppers -- so what is testable is that the two slots do not bleed.
  {
    program(0, 0, 0, /*CLUT8*/ 0, 64, 64, 0, 0, fx(1.0), 0, 0, fx(1.0), 0, 0, 3);
    program(1, 1, /*ALPHA*/ 1, /*RGB565*/ 1, 256, 128, 0, 0, fx(4.0), 0, 0, fx(4.0), 0, 0, 3);
    const S a = pixel(0, 3, 3, 0, 1);
    const S b = pixel(1, 3, 3, 0, 1);
    zhao::check(a.u == 3 && b.u == 12,
                "the two slots hold independent descriptors -- same pixel, "
                "different planes, different texels",
                1, (a.u == 3 && b.u == 12) ? 1 : 0);
    zhao::check(a.role == 0 && b.role == 1 && a.blend == 0 && b.blend == 1,
                "and each carries its own role and blend", 1, (a.role == 0 && b.role == 1) ? 1 : 0);
  }

  // ---- 5: THE REFUSALS, which is most of this contract ------------------
  {
    const uint32_t role_before = top.refused_role_o;
    program(0, /*role 2 -- reserved*/ 2, 0, 1, 64, 64, 0, 0, fx(1.0), 0, 0, fx(1.0), 0, 0, 3);
    const S s = pixel(0, 1, 1, 0, 1);
    zhao::check(top.refused_role_o == role_before + 1 && !s.valid,
                "role 2 is RESERVED: the descriptor is refused and the slot "
                "draws nothing -- a refused program that kept drawing the old "
                "plane would be worse than one that drew nothing",
                1, (top.refused_role_o == role_before + 1 && !s.valid) ? 1 : 0);

    const uint32_t blend_before = top.refused_blend_o;
    program(0, /*BACKDROP*/ 0, /*ALPHA*/ 1, 1, 64, 64, 0, 0, fx(1.0), 0, 0, fx(1.0), 0, 0, 3);
    const S t = pixel(0, 1, 1, 0, 1);
    zhao::check(top.refused_blend_o == blend_before + 1 && !t.valid,
                "a BACKDROP with a blend other than REPLACE is refused -- it "
                "sits beneath the resolved world, so there is nothing under it "
                "to blend with",
                1, (top.refused_blend_o == blend_before + 1) ? 1 : 0);
  }

  // ---- 6: the VIEW MASK ------------------------------------------------
  {
    const uint32_t skip_before = top.skipped_view_o;
    program(0, 0, 0, 1, 64, 64, 0, 0, fx(1.0), 0, 0, fx(1.0), 0, 0, /*mask*/ 2);
    const S s = pixel(0, 1, 1, 0, /*view*/ 1);
    zhao::check(!s.valid && top.skipped_view_o == skip_before + 1,
                "a plane not masked for this view emits nothing", 1,
                (!s.valid && top.skipped_view_o == skip_before + 1) ? 1 : 0);
  }

  // ---- 7: the WRAP ASSUMPTION, violated on purpose ---------------------
  // Repeat by one correction is exact while the coordinate leaves the range by
  // at most one size. A plane scaled so hard that one pixel steps several
  // widths breaks that, and the counter has to say so rather than the picture
  // tiling wrongly.
  {
    const uint32_t fail_before = top.wrap_fail_o;
    program(0, 0, 0, 1, /*w*/ 16, 16, 0, 0, fx(1.0), 0, 0, fx(1.0), 0, 0, 3);
    // x = 100 on a 16-wide plane is more than six widths out
    const S s = pixel(0, 100, 0, 0, 1);
    zhao::check(top.wrap_fail_o == fail_before + 1,
                "a coordinate more than one size out of range is COUNTED, not "
                "tiled wrongly -- one conditional correction is an assumption "
                "about callers, not a law",
                1, static_cast<int>(top.wrap_fail_o - fail_before));
    (void)s;
  }

  // ---- 8: NEAREST is structural ---------------------------------------
  // There is no fractional texel port on this block. A bilinear filter would
  // need one, so adding bilinear would have to change the interface -- which
  // is the enforcement. Asserted here so the absence is recorded as a decision
  // rather than as something nobody checked.
  {
    program(0, 0, 0, 1, 256, 128, 0, 0, fx(1.0), 0, 0, fx(1.0), fx(0.5), fx(0.5), 3);
    const S s = pixel(0, 0, 0, 0, 1);
    zhao::check(s.u == 0 && s.v == 0,
                "a coordinate halfway between texels resolves to ONE texel -- "
                "there is no fractional output, so there is nothing for a "
                "filter to weight",
                1, (s.u == 0 && s.v == 0) ? 1 : 0);
  }

  std::printf(
      "  %u pixels, %u role-refused, %u blend-refused, %u view-skipped, "
      "%u wrap failures\n",
      top.pixels_o, top.refused_role_o, top.refused_blend_o, top.skipped_view_o, top.wrap_fail_o);

  return zhao::report_and_exit("twod_plane_directed");
}
