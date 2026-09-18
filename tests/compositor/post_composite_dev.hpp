// post_composite_dev.hpp -- the fixture, the driver and the HAND-COMPUTED
// expectation for zhao_post_composite.
//
// ===========================================================================
// READ THIS BEFORE TRUSTING A GREEN RUN: THERE IS NO ORACLE HERE
// ===========================================================================
// `design/blocks.yml` declares POST.COMPOSITE's reference_model as
// `zref::PostComposite`. That symbol HAS NEVER BEEN WRITTEN.
// `tools/budget/refmodel_liveness.py` lists it among eleven phantom citations
// and says exactly what that costs:
//
//   "These blocks have NO differential test available. A directed test for one
//    of them checks self-consistency against contract prose, not agreement
//    with a ratified model."
//
// So this file is NOT a differential bench and must not be read as one. The
// `model_pixel` function below is the contract's prose transcribed into C++ by
// the same hand that wrote the RTL, which means IT CAN BE WRONG IN EXACTLY THE
// SAME WAY TWICE. Agreement between it and the RTL is evidence that the
// pipeline is internally consistent and that no stage was silently dropped. It
// is NOT evidence that the arithmetic is the ratified arithmetic.
//
// Two things reduce the damage, and both are deliberate:
//
//  1. The parts that DO have a ratified owner are not re-derived here. The
//     unit8 product and lerp are spec/qformats.md's frozen forms; the
//     displacement clamps belong to POST.GATHER/`zref::post` and are asserted
//     against the values that file fixes, not against numbers typed here.
//  2. Most of the checks in the directed bench are STRUCTURAL and do not depend
//     on `model_pixel` being right at all: that muting a stage changes the
//     picture, that the ink follows the world, that a displacement clamps
//     instead of wrapping, that the order is bloom-then-grade and not the
//     reverse. Those would still fail if the model and the RTL agreed on a
//     wrong number.
//
// When `zref::PostComposite` is written, this file's `model_pixel` should be
// DELETED, not kept beside it -- two models is how a wrong one survives.
// ===========================================================================
#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

#include "zhao_sim.hpp"

namespace pc {

// A small frame, chosen so a whole pass runs in a few thousand cycles. The
// width must stay comfortably above the nine-column ring lead (the RTL's
// elaboration guard wants >= 32) or the line-wrap half of the ring argument
// stops being exercised.
constexpr int W  = 48;
constexpr int H  = 24;
constexpr int CW = W / 4;   // 12 quarter-resolution cells across
constexpr int CH = H / 4;   // 6 down
constexpr int NPIX = W * H;

// ---- the frozen forms, transcribed from spec/qformats.md ------------------
inline uint8_t exp5(uint8_t v) { return static_cast<uint8_t>((v << 3) | (v >> 2)); }
inline uint8_t exp6(uint8_t v) { return static_cast<uint8_t>((v << 2) | (v >> 4)); }

inline uint16_t pack565(int r, int g, int b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3));
}

inline uint8_t unit_mul(uint8_t a, uint8_t b) {
  const uint32_t q = ((static_cast<uint32_t>(a) * b) + 128u) >> 8;
  return static_cast<uint8_t>(q > 255u ? 255u : q);
}

inline uint8_t unit_lerp(uint8_t dst, uint8_t src, uint8_t w) {
  const uint32_t p = (static_cast<uint32_t>(src) * w) +
                     (static_cast<uint32_t>(dst) * (256u - w)) + 128u;
  const uint32_t q = p >> 8;
  return static_cast<uint8_t>(q > 255u ? 255u : q);
}

inline uint8_t sat_add8(int a, int b) {
  const int s = a + b;
  return static_cast<uint8_t>(s > 255 ? 255 : s);
}

// ---------------------------------------------------------------------------
// The fixture
// ---------------------------------------------------------------------------
struct Frame {
  std::vector<uint16_t> src;   // NPIX, the resolved world over the backdrop
  std::vector<int8_t>   dx;    // CW*CH, POST.GATHER's ALREADY-COMBINED field
  std::vector<int8_t>   dy;
  std::vector<uint16_t> glow;  // CW*CH, RGB565
  std::vector<uint8_t>  ink;   // CW*CH, 1 bit

  Frame() : src(NPIX, 0), dx(CW * CH, 0), dy(CW * CH, 0),
            glow(CW * CH, 0), ink(CW * CH, 0) {}
};

struct Cfg {
  bool     duo         = false;
  int      view_split  = W / 2;

  bool     atm_en      = false;
  bool     atm_valid   = false;
  bool     atm_add     = false;
  uint16_t atm_rgb     = 0;
  uint8_t  atm_opacity = 0;

  uint8_t  bloom_gain  = 0;

  bool     grade_valid = false;
  uint8_t  curve_r[32] = {0};
  uint8_t  curve_g[64] = {0};
  uint8_t  curve_b[32] = {0};
  int16_t  m[9]        = {0, 0, 0, 0, 0, 0, 0, 0, 0};   // Q2.14, row major
  int16_t  bias[3]     = {0, 0, 0};

  uint16_t flash_rgb   = 0;
  uint8_t  flash_amt   = 0;

  uint16_t ink_rgb     = 0;

  bool     gd_present  = true;
  bool     gg_present  = true;

  std::vector<uint8_t> hud = std::vector<uint8_t>(NPIX, 0);  // 1 = HUD covers it
  uint16_t hud_rgb     = 0;
};

// A grading set that changes nothing once the result is repacked to RGB565:
// the curve is the identity on the 5/6/5 index and the matrix is 1.0.
inline void set_identity_grade(Cfg* c) {
  for (int i = 0; i < 32; ++i) c->curve_r[i] = exp5(static_cast<uint8_t>(i));
  for (int i = 0; i < 64; ++i) c->curve_g[i] = exp6(static_cast<uint8_t>(i));
  for (int i = 0; i < 32; ++i) c->curve_b[i] = exp5(static_cast<uint8_t>(i));
  c->m[0] = 16384; c->m[1] = 0;     c->m[2] = 0;
  c->m[3] = 0;     c->m[4] = 16384; c->m[5] = 0;
  c->m[6] = 0;     c->m[7] = 0;     c->m[8] = 16384;
  c->bias[0] = c->bias[1] = c->bias[2] = 0;
}

// A grading set that halves every channel -- non-commuting with bloom, which
// is what makes the stage-order check able to SEE the order it claims to test.
inline void set_halving_grade(Cfg* c) {
  for (int i = 0; i < 32; ++i) c->curve_r[i] = static_cast<uint8_t>(exp5(static_cast<uint8_t>(i)) >> 1);
  for (int i = 0; i < 64; ++i) c->curve_g[i] = static_cast<uint8_t>(exp6(static_cast<uint8_t>(i)) >> 1);
  for (int i = 0; i < 32; ++i) c->curve_b[i] = static_cast<uint8_t>(exp5(static_cast<uint8_t>(i)) >> 1);
  c->m[0] = 16384; c->m[1] = 0;     c->m[2] = 0;
  c->m[3] = 0;     c->m[4] = 16384; c->m[5] = 0;
  c->m[6] = 0;     c->m[7] = 0;     c->m[8] = 16384;
  c->bias[0] = c->bias[1] = c->bias[2] = 0;
}

// ---------------------------------------------------------------------------
// The hand-computed expectation. See the warning at the top of this file.
// `swap_grade_bloom` exists ONLY so the directed bench can prove it is able to
// tell the two orders apart; nothing may ever ship with it true.
// ---------------------------------------------------------------------------
inline uint16_t model_echo(const Frame& f, const Cfg& c, int x, int y,
                           bool swap_grade_bloom = false) {
  const int dxv = c.gd_present ? f.dx[(y >> 2) * CW + (x >> 2)] : 0;
  const int dyv = c.gd_present ? f.dy[(y >> 2) * CW + (x >> 2)] : 0;

  int lo = 0, hi = W - 1;
  if (c.duo) {
    if (x >= c.view_split) { lo = c.view_split; hi = W - 1; }
    else                   { lo = 0;            hi = c.view_split - 1; }
  }
  int xs = x + dxv; if (xs < lo) xs = lo; if (xs > hi) xs = hi;
  int ys = y + dyv; if (ys < 0)  ys = 0;  if (ys > H - 1) ys = H - 1;

  const uint16_t world = f.src[ys * W + xs];
  const uint16_t gl    = c.gg_present ? f.glow[(ys >> 2) * CW + (xs >> 2)] : 0;
  const int      ink   = c.gg_present ? f.ink[(ys >> 2) * CW + (xs >> 2)]  : 0;

  uint8_t r = exp5((world >> 11) & 31);
  uint8_t g = exp6((world >> 5) & 63);
  uint8_t b = exp5(world & 31);

  // stage 3 -- ATMOSPHERE
  if (c.atm_en && c.atm_valid) {
    const uint8_t ar = exp5((c.atm_rgb >> 11) & 31);
    const uint8_t ag = exp6((c.atm_rgb >> 5) & 63);
    const uint8_t ab = exp5(c.atm_rgb & 31);
    if (c.atm_add) {
      r = sat_add8(r, unit_mul(ar, c.atm_opacity));
      g = sat_add8(g, unit_mul(ag, c.atm_opacity));
      b = sat_add8(b, unit_mul(ab, c.atm_opacity));
    } else {
      r = unit_lerp(r, ar, c.atm_opacity);
      g = unit_lerp(g, ag, c.atm_opacity);
      b = unit_lerp(b, ab, c.atm_opacity);
    }
  }

  const uint8_t gr = exp5((gl >> 11) & 31);
  const uint8_t gg = exp6((gl >> 5) & 63);
  const uint8_t gb = exp5(gl & 31);

  auto do_bloom = [&]() {
    r = sat_add8(r, unit_mul(gr, c.bloom_gain));
    g = sat_add8(g, unit_mul(gg, c.bloom_gain));
    b = sat_add8(b, unit_mul(gb, c.bloom_gain));
  };
  auto do_grade = [&]() {
    if (!c.grade_valid) return;
    const int kr = c.curve_r[r >> 3];
    const int kg = c.curve_g[g >> 2];
    const int kb = c.curve_b[b >> 3];
    auto one = [&](int m0, int m1, int m2, int bias) -> uint8_t {
      const int64_t acc = static_cast<int64_t>(m0) * kr +
                          static_cast<int64_t>(m1) * kg +
                          static_cast<int64_t>(m2) * kb;
      int64_t q = (acc + 8192) >> 14;   // ONE round-half-up
      q += bias;                        // then the bias
      return static_cast<uint8_t>(q < 0 ? 0 : (q > 255 ? 255 : q));
    };
    const uint8_t nr = one(c.m[0], c.m[1], c.m[2], c.bias[0]);
    const uint8_t ng = one(c.m[3], c.m[4], c.m[5], c.bias[1]);
    const uint8_t nb = one(c.m[6], c.m[7], c.m[8], c.bias[2]);
    r = nr; g = ng; b = nb;
  };

  if (swap_grade_bloom) { do_grade(); do_bloom(); }
  else                  { do_bloom(); do_grade(); }

  // stage 8 -- FLASH
  r = unit_lerp(r, exp5((c.flash_rgb >> 11) & 31), c.flash_amt);
  g = unit_lerp(g, exp6((c.flash_rgb >> 5) & 63), c.flash_amt);
  b = unit_lerp(b, exp5(c.flash_rgb & 31), c.flash_amt);

  // stage 9 -- EXTERIOR INK, after the flash. This return value IS the
  // POST.ECHO tap: post-ink, pre-HUD.
  return ink ? c.ink_rgb : pack565(r, g, b);
}

inline uint16_t model_pixel(const Frame& f, const Cfg& c, int x, int y,
                            bool swap_grade_bloom = false) {
  const uint16_t e = model_echo(f, c, x, y, swap_grade_bloom);
  return c.hud[y * W + x] ? c.hud_rgb : e;   // stage 10 -- HUD, last
}

// ---------------------------------------------------------------------------
// The driver
// ---------------------------------------------------------------------------
struct Result {
  std::vector<uint16_t> rgb   = std::vector<uint16_t>(NPIX, 0);
  std::vector<uint16_t> echo  = std::vector<uint16_t>(NPIX, 0);
  std::vector<uint8_t>  seen  = std::vector<uint8_t>(NPIX, 0);
  int      emitted        = 0;
  int      duplicates     = 0;
  int      last_pulses    = 0;
  bool     completed      = false;
  uint32_t edge_clamps    = 0;
  uint32_t bloom_cells    = 0;
  uint32_t passes         = 0;
  uint32_t grade_missing  = 0;
  uint32_t plane_missing  = 0;
  uint32_t line_fill      = 0;
  uint32_t out_writes     = 0;
  uint32_t plane_reads    = 0;
  uint32_t hazard         = 0;
};

inline uint32_t rnd(uint32_t* s) {
  *s = (*s * 1664525u) + 1013904223u;
  return (*s >> 8);
}

template <typename Top>
inline void reset_dut(Top* t) {
  t->rst_n        = 0;
  t->frame_start_i = 0;
  t->s_valid_i    = 0;
  t->o_ready_i    = 1;
  t->curve_we_i   = 0;
  t->gd_present_i = 1;
  t->gg_present_i = 1;
  t->atm_valid_i  = 0;
  t->hud_valid_i  = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(*t);
  t->rst_n = 1;
  zhao::tick(*t);
}

// Load the three generated curves through the write port. They are assets, not
// runtime-computed, so they go in once and stay.
template <typename Top>
inline void load_curves(Top* t, const Cfg& c) {
  t->curve_we_i = 1;
  for (int i = 0; i < 128; ++i) {
    t->curve_addr_i = static_cast<uint8_t>(i);
    t->curve_data_i = (i < 32) ? c.curve_r[i]
                    : (i < 96) ? c.curve_g[i - 32]
                               : c.curve_b[i - 96];
    zhao::tick(*t);
  }
  t->curve_we_i = 0;
}

// One full pass. `stall_seed` of 0 means "downstream always ready"; anything
// else gates o_ready_i pseudo-randomly, which must change the cycle count and
// nothing else.
template <typename Top>
inline Result run_frame(Top* t, const Frame& f, const Cfg& c, uint32_t stall_seed = 0) {
  Result r;

  t->frame_w_i      = W;
  t->frame_h_i      = H;
  t->duo_i          = c.duo ? 1 : 0;
  t->view_split_i   = static_cast<uint16_t>(c.view_split);
  t->atm_en_i       = c.atm_en ? 1 : 0;
  t->atm_add_i      = c.atm_add ? 1 : 0;
  t->atm_rgb_i      = c.atm_rgb;
  t->atm_opacity_i  = c.atm_opacity;
  t->bloom_gain_i   = c.bloom_gain;
  t->grade_valid_i  = c.grade_valid ? 1 : 0;
  t->m00_i = static_cast<uint16_t>(c.m[0]); t->m01_i = static_cast<uint16_t>(c.m[1]);
  t->m02_i = static_cast<uint16_t>(c.m[2]); t->m10_i = static_cast<uint16_t>(c.m[3]);
  t->m11_i = static_cast<uint16_t>(c.m[4]); t->m12_i = static_cast<uint16_t>(c.m[5]);
  t->m20_i = static_cast<uint16_t>(c.m[6]); t->m21_i = static_cast<uint16_t>(c.m[7]);
  t->m22_i = static_cast<uint16_t>(c.m[8]);
  t->bias_r_i = static_cast<uint16_t>(c.bias[0] & 0x1FF);
  t->bias_g_i = static_cast<uint16_t>(c.bias[1] & 0x1FF);
  t->bias_b_i = static_cast<uint16_t>(c.bias[2] & 0x1FF);
  t->flash_rgb_i    = c.flash_rgb;
  t->flash_amt_i    = c.flash_amt;
  t->ink_rgb_i      = c.ink_rgb;
  t->hud_rgb_i      = c.hud_rgb;
  t->gd_present_i   = c.gd_present ? 1 : 0;
  t->gg_present_i   = c.gg_present ? 1 : 0;
  t->s_valid_i      = 0;
  t->o_ready_i      = 1;

  t->frame_start_i = 1;
  zhao::tick(*t);
  t->frame_start_i = 0;

  int in_idx = 0;
  // The gather ports are 1-cycle-latency masters (RASTER.RESOLVE's convention):
  // the address presented in cycle N is answered in cycle N+1, so the driver
  // carries the previous cycle's address.
  int gd_cx_p = 0, gd_cy_p = 0, gg_cx_p = 0, gg_cy_p = 0;
  int hud_x_p = 0, hud_y_p = 0;
  bool hud_v_p = false, atm_v_p = false;
  uint32_t rs = stall_seed ? stall_seed : 1u;

  for (int cyc = 0; cyc < 60000 && !r.completed; ++cyc) {
    t->eval();

    // ---- gather port A: displacement, at the UNDISPLACED pixel ----------
    const int gd_cx_n = t->gd_cx_o;
    const int gd_cy_n = t->gd_cy_o;
    const int dcell = (gd_cy_p * CW) + gd_cx_p;
    t->gd_dx_i = static_cast<uint8_t>(f.dx[dcell]);
    t->gd_dy_i = static_cast<uint8_t>(f.dy[dcell]);
    t->eval();   // gg_cx_o/gg_cy_o depend on the displacement just supplied

    // ---- gather port B: glow and ink, at the DISPLACED coordinate -------
    const int gg_cx_n = t->gg_cx_o;
    const int gg_cy_n = t->gg_cy_o;
    const int gcell = (gg_cy_p * CW) + gg_cx_p;
    t->gg_glow_i = f.glow[gcell];
    t->gg_ink_i  = f.ink[gcell];

    // ---- the stage-aligned side inputs ----------------------------------
    t->atm_valid_i = (c.atm_valid && atm_v_p) ? 1 : 0;
    t->hud_valid_i = (hud_v_p && c.hud[(hud_y_p * W) + hud_x_p]) ? 1 : 0;

    const bool atm_v_n = (t->atm_req_v_o != 0);
    const bool hud_v_n = (t->hud_req_v_o != 0);
    const int  hud_x_n = t->hud_req_x_o;
    const int  hud_y_n = t->hud_req_y_o;

    // ---- the world stream -----------------------------------------------
    t->s_valid_i = (in_idx < NPIX) ? 1 : 0;
    t->s_rgb_i   = (in_idx < NPIX) ? f.src[in_idx] : 0;
    t->o_ready_i = stall_seed ? ((rnd(&rs) & 3u) != 0u ? 1 : 0) : 1;

    t->eval();

    const bool accepted_in  = (t->s_valid_i != 0) && (t->s_ready_o != 0);
    const bool accepted_out = (t->o_valid_o != 0) && (t->o_ready_i != 0);
    if (accepted_out) {
      const int ox = t->o_x_o, oy = t->o_y_o;
      if (ox >= 0 && ox < W && oy >= 0 && oy < H) {
        const int idx = (oy * W) + ox;
        if (r.seen[idx]) ++r.duplicates;
        r.seen[idx] = 1;
        r.rgb[idx]  = t->o_rgb_o;
        r.echo[idx] = t->echo_rgb_o;
      }
      ++r.emitted;
      if (t->o_last_o) { ++r.last_pulses; r.completed = true; }
    }

    // Did the machine actually advance? Every side input is a ONE-CYCLE-LATENCY
    // response to a request, and a response must be HELD until the step that
    // consumes it happens -- exactly like a synchronous memory whose address is
    // being held. Advancing the driver's shadow through a stall presents the
    // NEXT pixel's atmosphere and HUD to the pixel still sitting in the stage,
    // which is a driver bug that looks exactly like a compositor bug.
    //
    // `step` is not a port, and it does not need to be: it is reconstructible
    // from the handshake alone, which is the test that the handshake is
    // complete.
    //   pipe_en   = !o_valid || o_ready
    //   s_ready_o = pipe_en && in_active     -> in_active = pipe_en && s_ready
    //   step      = pipe_en && (in_accept || !in_active)
    const bool pipe_en   = (t->o_valid_o == 0) || (t->o_ready_i != 0);
    const bool in_active = pipe_en && (t->s_ready_o != 0);
    const bool stepped   = pipe_en && (accepted_in || !in_active);

    zhao::tick(*t);

    if (accepted_in) ++in_idx;
    if (stepped) {
      gd_cx_p = gd_cx_n; gd_cy_p = gd_cy_n;
      gg_cx_p = gg_cx_n; gg_cy_p = gg_cy_n;
      atm_v_p = atm_v_n;
      hud_v_p = hud_v_n; hud_x_p = hud_x_n; hud_y_p = hud_y_n;
    }
  }

  // a few idle cycles so the pass tick lands
  t->s_valid_i = 0;
  t->o_ready_i = 1;
  for (int i = 0; i < 8; ++i) zhao::tick(*t);

  r.edge_clamps   = t->displacement_edge_clamps_o;
  r.bloom_cells   = t->bloom_cells_contributing_o;
  r.passes        = t->passes_completed_o;
  r.grade_missing = t->grading_table_missing_o;
  r.plane_missing = t->plane_missing_o;
  r.line_fill     = t->line_fill_writes_o;
  r.out_writes    = t->output_writes_o;
  r.plane_reads   = t->plane_reads_o;
  r.hazard        = t->ring_hazard_o;
  return r;
}

// A deterministic world with no two columns alike, so a one-pixel horizontal
// shift is visible rather than plausible.
inline Frame base_frame() {
  Frame f;
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      f.src[(y * W) + x] = pack565((x * 5) & 0xFF, (y * 11) & 0xFF, ((x * 3) + (y * 7)) & 0xFF);
  return f;
}

// How many of a frame's pixels the RTL and the hand-computed expectation
// disagree on. Zero is the only acceptable answer for the shipping order.
template <typename R>
inline int diff_count(const R& r, const Frame& f, const Cfg& c, bool swap = false) {
  int n = 0;
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      if (r.rgb[(y * W) + x] != model_pixel(f, c, x, y, swap)) ++n;
  return n;
}

}  // namespace pc
