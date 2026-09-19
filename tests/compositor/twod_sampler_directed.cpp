// twod_sampler_directed.cpp -- the seam between a texel REQUEST and a COLOUR.
//
// ---------------------------------------------------------------------------
// WHAT THIS BENCH IS AND IS NOT
// ---------------------------------------------------------------------------
// It is NOT a differential. There is no oracle for this block because there is
// no colour arithmetic in it to check against one: a CLUT8 entry is RGB565, an
// RGB565 texel is RGB565, and POST.COMPOSITE consumes RGB565. What can be
// wrong here is ADDRESSING, PAIRING, RESIDENCY and BACKPRESSURE, and those are
// checked against hand-computed expectations and against the two producers'
// own published handshakes.
//
// TWOD.PLANE IS MODELLED, NOT INSTANTIATED, and the model is one sentence of
// that block's contract: `p_ready_o = !s_valid_o || s_ready_i`, one register
// deep, and a pixel it SKIPS produces no sample at all. Modelling it is the
// point -- the pairing this block performs is an assumption about exactly that
// handshake, and a model lets the bench violate it on purpose.
//
// POST.COMPOSITE is modelled the same way and for the same reason: its `atm_*`
// convention is "address out in cycle N, data in cycle N+1, and both HOLD
// through a stall". A model that re-presents the same address while stalled is
// what proves the hold rather than assuming it.
//
// EVERY COUNTER THIS BENCH ASSERTS IS ZERO IS ALSO FIRED, in case 9. None of
// them needed a committed mutant: every guard here is reachable with legal
// stimulus, because every one of them watches a CALLER's mistake rather than
// an internal invariant. `pair_lost_o` is the closest to unreachable and it is
// fired by presenting a plane sample nobody walked for -- which a bench can do
// and a composed console cannot.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_twod_sampler.h"

#include "zhao_sim.hpp"

namespace {

// The block's defaults, restated here so a parameter change breaks the bench
// loudly instead of quietly changing what it measures.
constexpr int kPalSlots = 4;
constexpr int kAtmLines = 4;

// The page this bench programs: 8 words per row (lstride 3), 8 rows
// (lheight 3). CLUT8 packs two texels per word, so a row is 16 texels wide.
constexpr int kLStride = 3;
constexpr int kLHeight = 3;
constexpr int kRowWords = 1 << kLStride;
constexpr int kRowTexelsClut = kRowWords * 2;
constexpr int kPageRows = 1 << kLHeight;

// Binding slots, per the RTL: plane uses its ROLE, sprite uses 4 + src_id[1:0].
constexpr int kBindPlaneBackdrop = 0;
constexpr int kBindPlaneAtm = 1;
constexpr int kBindSpriteBase = 4;

constexpr uint16_t kRefusedRgb = 0xF81F;

// index(u, v) for the CLUT8 page, and the palette entry it resolves to.
uint8_t clut_index(int u, int v) { return static_cast<uint8_t>((v * 37 + u * 11 + 3) & 0xFF); }
uint16_t pal_entry(int slot, uint8_t idx) {
  return static_cast<uint16_t>(((slot & 3) << 13) ^ (0x1234 + idx * 7));
}
uint16_t expected_clut(int u, int v, int slot) { return pal_entry(slot, clut_index(u, v)); }

// The RGB565 page laid over the same words: texel (u, v) IS word (v<<3) + u.
uint16_t rgb565_texel(int u, int v) {
  return static_cast<uint16_t>(0xA000 ^ (v * 0x0123) ^ (u * 0x0045));
}

struct Bench {
  Vzhao_twod_sampler& t;

  // --- the TWOD.PLANE model ------------------------------------------------
  bool pend = false;      // a sample is sitting in the plane's one register
  int pend_x = 0, pend_y = 0;
  bool plane_draws = true;  // false models a view-masked / disabled slot
  int plane_opacity = 0xC0;
  int plane_blend = 1;      // 0 REPLACE, 1 ALPHA, 2 ADD
  int plane_role = 1;       // ATMOSPHERE
  int plane_fmt = 0;        // 0 CLUT8, 1 RGB565
  int plane_pal = 0;
  int plane_u_bias = 0;     // added to u before it leaves the model (wrap probe)
  bool plane_free_run = false;  // feed samples with no walk behind them

  // --- the POST.COMPOSITE model --------------------------------------------
  bool creq = false;
  int cx = 0, cy = 0;

  explicit Bench(Vzhao_twod_sampler& top) : t(top) {}

  void drive_plane_response() {
    const bool v = pend || plane_free_run;
    t.pl_valid_i = v ? 1 : 0;
    const int u = pend_x + plane_u_bias;
    t.pl_texel_u_i = static_cast<uint16_t>(u);
    t.pl_texel_v_i = static_cast<uint16_t>(pend_y);
    t.pl_format_i = plane_fmt;
    t.pl_palette_i = plane_pal;
    t.pl_blend_i = plane_blend;
    t.pl_opacity_i = plane_opacity;
    t.pl_role_i = plane_role;
  }

  // One cycle of the whole arrangement. Returns true if the compositor model
  // got a valid atmosphere cell this cycle (checked by the caller).
  void cycle() {
    drive_plane_response();
    t.atm_req_v_i = creq ? 1 : 0;
    t.atm_req_x_i = cx;
    t.atm_req_y_i = cy;
    t.eval();

    // The plane's own ready: it accepts a new pixel only when its register is
    // free. Straight out of zhao_twod_plane.sv.
    t.pw_ready_i = (!pend || t.pl_ready_o) ? 1 : 0;
    t.eval();

    const bool fire = (t.pw_valid_o != 0) && (t.pw_ready_i != 0);
    const int fx = t.pw_x_o;
    const int fy = t.pw_y_o;
    const bool consumed = pend && (t.pl_ready_o != 0);

    zhao::tick(t);

    if (consumed || !pend) {
      pend = fire && plane_draws;
      if (fire) {
        pend_x = fx;
        pend_y = fy;
      }
    }
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_twod_sampler top;
  Bench b(top);

  auto zero_inputs = [&]() {
    top.frame_start_i = 0;
    top.frame_w_i = 8;
    top.frame_h_i = 6;
    top.ld_page_we_i = 0;
    top.ld_pal_we_i = 0;
    top.ld_bind_we_i = 0;
    top.atm_slot_i = 1;
    top.line_scroll_i = 0;
    top.pw_ready_i = 0;
    top.pl_valid_i = 0;
    top.sp_valid_i = 0;
    top.sp_x_i = 0;
    top.sp_y_i = 0;
    top.sp_u_i = 0;
    top.sp_v_i = 0;
    top.sp_format_i = 0;
    top.sp_palette_i = 0;
    top.sp_tint_i = 0xFFFF;
    top.sp_blend_i = 0;
    top.sp_order_i = 0;
    top.sp_src_id_i = 0;
    top.sp_last_i = 0;
    top.sc_ready_i = 1;
    top.atm_req_v_i = 0;
    top.atm_req_x_i = 0;
    top.atm_req_y_i = 0;
  };

  auto reset = [&]() {
    zero_inputs();
    top.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
    b.pend = false;
    b.creq = false;
    b.cx = 0;
    b.cy = 0;
  };

  auto write_page = [&](int addr, uint16_t data) {
    top.ld_page_we_i = 1;
    top.ld_page_addr_i = addr;
    top.ld_page_data_i = data;
    zhao::tick(top);
    top.ld_page_we_i = 0;
  };
  auto write_pal = [&](int addr, uint16_t data) {
    top.ld_pal_we_i = 1;
    top.ld_pal_addr_i = addr;
    top.ld_pal_data_i = data;
    zhao::tick(top);
    top.ld_pal_we_i = 0;
  };
  auto write_bind = [&](int sel, int base, int lstride, int lheight) {
    top.ld_bind_we_i = 1;
    top.ld_bind_sel_i = sel;
    top.ld_bind_base_i = base;
    top.ld_bind_lstride_i = lstride;
    top.ld_bind_lheight_i = lheight;
    zhao::tick(top);
    top.ld_bind_we_i = 0;
  };

  reset();

  // ---- the assets -------------------------------------------------------
  // The CLUT8 page at word 0 and an RGB565 page at word 256, both 8x8 words.
  constexpr int kClutBase = 0;
  constexpr int kRgbBase = 256;
  for (int v = 0; v < kPageRows; ++v) {
    for (int w = 0; w < kRowWords; ++w) {
      const uint8_t lo = clut_index(w * 2, v);
      const uint8_t hi = clut_index(w * 2 + 1, v);
      write_page(kClutBase + v * kRowWords + w, static_cast<uint16_t>((hi << 8) | lo));
    }
    for (int u = 0; u < kRowWords; ++u) {
      write_page(kRgbBase + v * kRowWords + u, rgb565_texel(u, v));
    }
  }
  for (int s = 0; s < kPalSlots; ++s)
    for (int i = 0; i < 256; ++i) write_pal(s * 256 + i, pal_entry(s, static_cast<uint8_t>(i)));

  write_bind(kBindPlaneAtm, kClutBase, kLStride, kLHeight);
  write_bind(kBindSpriteBase + 0, kClutBase, kLStride, kLHeight);
  write_bind(kBindSpriteBase + 1, kRgbBase, kLStride, kLHeight);
  // kBindPlaneBackdrop (role 0) is left UNPROGRAMMED on purpose: case 9 uses it
  // to fire bind_missing_o.

  // =======================================================================
  // 1 -- a whole atmosphere frame, walked by this block and read back on
  //      POST.COMPOSITE's own convention.
  // =======================================================================
  const int W = 8, H = 6;
  auto run_frame = [&](bool check_pixels, int* bad_colour, int* bad_valid, int max_cycles) {
    top.frame_start_i = 1;
    b.creq = false;
    zhao::tick(top);
    top.frame_start_i = 0;

    b.cx = 0;
    b.cy = 0;
    b.creq = true;
    int got = 0;
    for (int c = 0; c < max_cycles && b.cy < H; ++c) {
      const int rx = b.cx, ry = b.cy;
      b.cycle();
      if (top.atm_valid_o) {
        ++got;
        if (check_pixels) {
          const uint16_t want = expected_clut(rx, ry, 0);
          if (top.atm_rgb_o != want) ++(*bad_colour);
          if (top.atm_opacity_o != 0xC0) ++(*bad_colour);
          if (top.atm_add_o != 0) ++(*bad_colour);
        }
        if (++b.cx >= W) {
          b.cx = 0;
          ++b.cy;
        }
      } else if (check_pixels && bad_valid != nullptr) {
        // not an error: the compositor holds its address until the line is
        // resident. Counted so a frame that NEVER becomes resident is visible.
        ++(*bad_valid);
      }
    }
    b.creq = false;
    return got;
  };

  {
    int bad_colour = 0, held = 0;
    const int got = run_frame(true, &bad_colour, &held, 20000);
    zhao::check(got == W * H, "every pixel of the frame came back exactly once", W * H, got);
    zhao::check(bad_colour == 0, "CLUT8 atmosphere colour, opacity and add bit are the page's",
                0, bad_colour);
    zhao::check(top.plane_samples_o == static_cast<uint32_t>(W * H),
                "one plane sample per walked pixel", W * H, top.plane_samples_o);
    zhao::check(top.clut8_samples_o == static_cast<uint32_t>(W * H),
                "all of them went down the CLUT8 path", W * H, top.clut8_samples_o);
    zhao::check(top.pair_lost_o == 0, "no plane sample arrived unpaired", 0, top.pair_lost_o);
    zhao::check(top.skipped_fill_o == 0, "the plane skipped nothing", 0, top.skipped_fill_o);
    zhao::check(top.page_oob_o == 0, "no address left the page", 0, top.page_oob_o);
    zhao::check(top.texel_wrapped_o == 0, "no coordinate needed masking", 0, top.texel_wrapped_o);
    zhao::check(top.bind_missing_o == 0, "the binding was programmed", 0, top.bind_missing_o);
    zhao::check(held > 0,
                "the compositor DID have to hold its address -- the ring is finite and this "
                "bench exercised the wait rather than assuming it away",
                1, held > 0 ? 1 : 0);
  }

  // =======================================================================
  // 2 -- atm_en_o is the PREVIOUS frame's verdict, and it turns itself off
  //      when the plane draws nothing.
  // =======================================================================
  {
    // atm_en_o is latched AT a frame edge, so it cannot be read between
    // frames: the verdict of frame 1 becomes visible when frame 2 starts.
    // Reading it earlier is reading the value frame 1 itself ran under, which
    // is the reset value, and a bench that asserted on it would be asserting
    // the wrong frame's answer.
    b.plane_draws = false;          // a view-masked slot: accepted, no sample
    int bad = 0, held = 0;
    run_frame(false, &bad, &held, 20000);
    zhao::check(top.atm_en_o == 1,
                "after a frame that produced atmosphere, atm_en_o is asserted for the next", 1,
                top.atm_en_o);
    // The frame cannot complete (nothing is resident), so run_frame times out;
    // what matters is that the next frame edge turns the sheet off.
    top.frame_start_i = 1;
    zhao::tick(top);
    top.frame_start_i = 0;
    zhao::check(top.atm_en_o == 0,
                "a frame in which the plane drew nothing turns the sheet off for the next", 0,
                top.atm_en_o);
    zhao::check(top.skipped_fill_o > 0,
                "every skipped pixel wrote a transparent cell instead of leaving the last "
                "line's colour behind",
                1, top.skipped_fill_o > 0 ? 1 : 0);
    b.plane_draws = true;
  }

  // =======================================================================
  // 3 -- THE HOLD. POST.COMPOSITE's contract is that a stalled request keeps
  //      its answer. Re-present the same address for many cycles and require
  //      the same word every time.
  // =======================================================================
  {
    reset();
    for (int v = 0; v < kPageRows; ++v)
      for (int w = 0; w < kRowWords; ++w) {
        const uint8_t lo = clut_index(w * 2, v);
        const uint8_t hi = clut_index(w * 2 + 1, v);
        write_page(kClutBase + v * kRowWords + w, static_cast<uint16_t>((hi << 8) | lo));
      }
    for (int i = 0; i < 256; ++i) write_pal(i, pal_entry(0, static_cast<uint8_t>(i)));
    write_bind(kBindPlaneAtm, kClutBase, kLStride, kLHeight);

    int bad = 0, held = 0;
    run_frame(false, &bad, &held, 20000);

    // The LAST line, not an early one. The ring holds ATM_LINES lines and line
    // 1 shares its slot with line 5, so by the end of the frame line 1 is
    // legitimately gone -- a bench that held it would be asserting that a
    // finite ring is infinite.
    b.cx = 3;
    b.cy = H - 1;
    b.creq = true;
    uint16_t first = 0;
    int mismatches = 0;
    for (int c = 0; c < 12; ++c) {
      b.cycle();
      if (c == 0) first = top.atm_rgb_o;
      if (top.atm_valid_o == 0) ++mismatches;
      if (top.atm_rgb_o != first) ++mismatches;
    }
    b.creq = false;
    zhao::check(mismatches == 0,
                "a held request returns the same cell every cycle -- the synchronous read IS "
                "the hold POST.COMPOSITE's header asks for",
                0, mismatches);
    zhao::check(first == expected_clut(3, H - 1, 0), "and it is the right cell",
                expected_clut(3, H - 1, 0), first);
  }

  // =======================================================================
  // 4 -- the sprite request shape, both formats, and the fx16 fraction that
  //      must not change the answer.
  // =======================================================================
  auto sprite = [&](int x, int y, double u, double v, int fmt, int src, int pal, uint16_t tint,
                    int max_wait = 64) {
    top.sp_valid_i = 1;
    top.sp_x_i = x;
    top.sp_y_i = y;
    top.sp_u_i = static_cast<int32_t>(u * 65536.0);
    top.sp_v_i = static_cast<int32_t>(v * 65536.0);
    top.sp_format_i = fmt;
    top.sp_palette_i = pal;
    top.sp_tint_i = tint;
    top.sp_blend_i = 1;
    top.sp_order_i = 7;
    top.sp_src_id_i = src;
    top.sp_last_i = 1;
    int w = 0;
    top.eval();
    while (!top.sp_ready_o && w < max_wait) {
      zhao::tick(top);
      top.eval();
      ++w;
    }
    zhao::tick(top);
    top.sp_valid_i = 0;
    // drain to the output stage
    for (int i = 0; i < 4 && !top.sc_valid_o; ++i) zhao::tick(top);
    top.eval();
  };

  {
    reset();
    for (int v = 0; v < kPageRows; ++v) {
      for (int w = 0; w < kRowWords; ++w) {
        const uint8_t lo = clut_index(w * 2, v);
        const uint8_t hi = clut_index(w * 2 + 1, v);
        write_page(kClutBase + v * kRowWords + w, static_cast<uint16_t>((hi << 8) | lo));
      }
      for (int u = 0; u < kRowWords; ++u)
        write_page(kRgbBase + v * kRowWords + u, rgb565_texel(u, v));
    }
    for (int s = 0; s < kPalSlots; ++s)
      for (int i = 0; i < 256; ++i) write_pal(s * 256 + i, pal_entry(s, static_cast<uint8_t>(i)));
    write_bind(kBindSpriteBase + 0, kClutBase, kLStride, kLHeight);
    write_bind(kBindSpriteBase + 1, kRgbBase, kLStride, kLHeight);

    // CLUT8 through src_id 0, with a fraction that must be discarded.
    int bad = 0;
    for (int v = 0; v < 6; ++v)
      for (int u = 0; u < 9; ++u) {
        sprite(100 + u, 50 + v, u + 0.75, v + 0.25, /*CLUT8*/ 0, /*src*/ 0, /*pal*/ 2, 0xFFFF);
        if (!top.sc_valid_o) ++bad;
        if (top.sc_rgb_o != expected_clut(u, v, 2)) ++bad;
        if (top.sc_x_o != 100 + u || top.sc_y_o != 50 + v) ++bad;
        if (top.sc_order_o != 7 || top.sc_last_o != 1) ++bad;
      }
    zhao::check(bad == 0, "sprite CLUT8: colour, palette slot and screen position all arrive", 0,
                bad);

    // RGB565 through src_id 1: the texel word IS the colour, no arithmetic.
    int bad2 = 0;
    for (int v = 0; v < 6; ++v)
      for (int u = 0; u < 8; ++u) {
        sprite(0, 0, u + 0.99, v + 0.01, /*RGB565*/ 1, /*src*/ 1, /*pal*/ 0, 0xFFFF);
        if (top.sc_rgb_o != rgb565_texel(u, v)) ++bad2;
      }
    zhao::check(bad2 == 0, "sprite RGB565: the texel word is the colour, unmodified", 0, bad2);
    zhao::check(top.rgb565_samples_o == 48, "and the format counters split the two paths", 48,
                top.rgb565_samples_o);
  }

  // =======================================================================
  // 5 -- BACKPRESSURE. The sprite stream stalls and NOTHING is lost.
  // =======================================================================
  {
    const uint32_t before = top.sprite_samples_o;
    top.sc_ready_i = 0;

    // Offer eight sprite pixels while the output is jammed. The block must
    // refuse them at its own input rather than drop them at its output.
    int offered = 0, accepted = 0;
    for (int i = 0; i < 8; ++i) {
      top.sp_valid_i = 1;
      top.sp_x_i = 200 + i;
      top.sp_y_i = 60;
      top.sp_u_i = static_cast<int32_t>((i % 8) * 65536);
      top.sp_v_i = 0;
      top.sp_format_i = 1;
      top.sp_src_id_i = 1;
      top.sp_palette_i = 0;
      top.sp_tint_i = 0xFFFF;
      top.sp_last_i = 0;
      ++offered;
      // CHANGE AN INPUT, THEN EVAL, THEN TEST. Written the other way round --
      // release `sc_ready_i` at the bottom of the loop and test the ready that
      // was computed before it -- the loop takes one more tick with the new
      // input already applied, accepts there, and then accepts AGAIN on the
      // way out. One offered pixel, two samples, and the only symptom is a
      // count that is one too high.
      int guard = 0;
      bool got = false;
      while (guard < 40) {
        if (guard == 20) top.sc_ready_i = 1;  // release halfway through
        top.eval();
        if (top.sp_ready_o) {
          got = true;
          break;
        }
        zhao::tick(top);
        ++guard;
      }
      if (got) {
        zhao::tick(top);
        ++accepted;
      }
      top.sp_valid_i = 0;
    }
    top.sc_ready_i = 1;
    for (int i = 0; i < 8; ++i) zhao::tick(top);

    zhao::check(accepted == offered, "every offered sprite pixel was eventually accepted",
                offered, accepted);
    zhao::check(top.sprite_stalls_o > 0,
                "and the refusal was a REFUSAL -- sprite_stalls_o saw the jam", 1,
                top.sprite_stalls_o > 0 ? 1 : 0);
    zhao::check(top.sprite_samples_o == before + static_cast<uint32_t>(accepted),
                "exactly as many colours came out as requests went in", before + accepted,
                top.sprite_samples_o);
  }

  // =======================================================================
  // 6 -- the plane still wins the arbiter while the sprite is offering.
  // =======================================================================
  {
    reset();
    write_bind(kBindPlaneAtm, kClutBase, kLStride, kLHeight);
    write_bind(kBindSpriteBase + 1, kRgbBase, kLStride, kLHeight);
    for (int i = 0; i < 256; ++i) write_pal(i, pal_entry(0, static_cast<uint8_t>(i)));

    top.sp_valid_i = 1;
    top.sp_format_i = 1;
    top.sp_src_id_i = 1;
    top.sp_x_i = 0;
    top.sp_y_i = 0;
    top.sp_u_i = 0;
    top.sp_v_i = 0;
    top.sp_tint_i = 0xFFFF;

    int bad = 0, held = 0;
    const int got = run_frame(false, &bad, &held, 20000);
    top.sp_valid_i = 0;
    zhao::check(got == W * H,
                "the atmosphere frame still completed with a sprite pushing on every cycle",
                W * H, got);
    zhao::check(top.plane_samples_o == static_cast<uint32_t>(W * H),
                "and the plane's samples were not displaced by it", W * H, top.plane_samples_o);
  }

  // =======================================================================
  // 7 -- RESIDENCY. A line that has not been prepared is NOT returned as if
  //      it had been. This is the check that a stale ring cell cannot pass
  //      itself off as this line's.
  // =======================================================================
  {
    reset();
    write_bind(kBindPlaneAtm, kClutBase, kLStride, kLHeight);
    top.frame_start_i = 1;
    zhao::tick(top);
    top.frame_start_i = 0;

    // Ask for the LAST line first. Nothing has been walked.
    b.cx = 0;
    b.cy = H - 1;
    b.creq = true;
    int wrongly_valid = 0;
    for (int c = 0; c < 8; ++c) {
      b.cycle();
      if (top.atm_valid_o) ++wrongly_valid;
    }
    b.creq = false;
    zhao::check(wrongly_valid == 0,
                "a line the walk has not reached is reported ABSENT, not returned stale", 0,
                wrongly_valid);
  }

  // =======================================================================
  // 8 -- the walk stalls rather than overwriting a line the compositor still
  //      needs. ATM_LINES is finite and this is what makes that safe.
  // =======================================================================
  {
    reset();
    write_bind(kBindPlaneAtm, kClutBase, kLStride, kLHeight);
    top.frame_start_i = 1;
    zhao::tick(top);
    top.frame_start_i = 0;
    b.creq = false;
    for (int c = 0; c < 600; ++c) b.cycle();
    zhao::check(top.walk_stalls_o > 0,
                "with the compositor silent, the walk fills its ring and then STOPS", 1,
                top.walk_stalls_o > 0 ? 1 : 0);
    zhao::check(top.plane_samples_o <= static_cast<uint32_t>(kAtmLines * W),
                "having written no more than the ring can hold", kAtmLines * W,
                top.plane_samples_o);
  }

  // =======================================================================
  // 9 -- THE POSITIVE CONTROLS. Every counter asserted zero above is fired
  //      here on purpose. A detector that has never been seen to move is a
  //      claim, not evidence.
  // =======================================================================
  {
    reset();
    write_bind(kBindPlaneAtm, /*base*/ 8180, kLStride, kLHeight);  // base near the top
    write_bind(kBindSpriteBase + 1, kRgbBase, kLStride, kLHeight);

    // (a) pair_lost_o -- a plane sample with no walk behind it. The composed
    //     console cannot produce this; a bench can, which is the point.
    b.plane_free_run = true;
    b.pend_x = 2;
    b.pend_y = 1;
    for (int c = 0; c < 6; ++c) b.cycle();
    b.plane_free_run = false;
    for (int c = 0; c < 4; ++c) b.cycle();
    zhao::check(top.pair_lost_o > 0, "FIRED: pair_lost_o sees a sample nobody asked for", 1,
                top.pair_lost_o > 0 ? 1 : 0);

    // (b) page_oob_o and (c) texel_wrapped_o -- a coordinate past the page.
    const uint32_t oob0 = top.page_oob_o;
    const uint32_t wrap0 = top.texel_wrapped_o;
    b.plane_free_run = true;
    b.pend_x = 900;   // far outside a 16-texel row: the mask changes it
    b.pend_y = 7;     // and with base 8180 the masked address still leaves the store
    for (int c = 0; c < 8; ++c) b.cycle();
    b.plane_free_run = false;
    for (int c = 0; c < 4; ++c) b.cycle();
    zhao::check(top.texel_wrapped_o > wrap0, "FIRED: texel_wrapped_o sees the mask bite", 1,
                top.texel_wrapped_o > wrap0 ? 1 : 0);
    zhao::check(top.page_oob_o > oob0, "FIRED: page_oob_o sees an address past the store", 1,
                top.page_oob_o > oob0 ? 1 : 0);

    // (d) bind_missing_o -- role 0's binding was never programmed in this run.
    const uint32_t bm0 = top.bind_missing_o;
    b.plane_role = 0;
    b.plane_free_run = true;
    b.pend_x = 1;
    b.pend_y = 1;
    for (int c = 0; c < 6; ++c) b.cycle();
    b.plane_free_run = false;
    b.plane_role = 1;
    for (int c = 0; c < 4; ++c) b.cycle();
    zhao::check(top.bind_missing_o > bm0,
                "FIRED: bind_missing_o sees a binding slot nobody programmed", 1,
                top.bind_missing_o > bm0 ? 1 : 0);

    // (e) fmt_refused_o and the key colour that goes with it.
    sprite(0, 0, 1.0, 1.0, /*format 3 = ARGB1555*/ 3, /*src*/ 1, 0, 0xFFFF);
    zhao::check(top.fmt_refused_o > 0, "FIRED: fmt_refused_o sees a format past the ceiling", 1,
                top.fmt_refused_o > 0 ? 1 : 0);
    zhao::check(top.sc_rgb_o == kRefusedRgb,
                "and a refused format samples the key colour, not a plausible one", kRefusedRgb,
                top.sc_rgb_o);

    // (f) pal_refused_o -- a palette id this instance does not own.
    write_bind(kBindSpriteBase + 0, kClutBase, kLStride, kLHeight);
    sprite(0, 0, 1.0, 1.0, /*CLUT8*/ 0, /*src*/ 0, /*palette*/ 9, 0xFFFF);
    zhao::check(top.pal_refused_o > 0, "FIRED: pal_refused_o sees a palette slot past PAL_SLOTS",
                1, top.pal_refused_o > 0 ? 1 : 0);

    // (g) tint_unapplied_o -- the omission this block declares rather than
    //     hides. A non-unity tint is forwarded and counted.
    sprite(0, 0, 1.0, 1.0, /*RGB565*/ 1, /*src*/ 1, 0, 0x8421);
    zhao::check(top.tint_unapplied_o > 0,
                "FIRED: tint_unapplied_o counts every tint this block forwards rather than "
                "applies",
                1, top.tint_unapplied_o > 0 ? 1 : 0);
    zhao::check(top.sc_tint_o == 0x8421, "and the tint itself reaches the consumer intact",
                0x8421, top.sc_tint_o);

    // (h) atm_underrun_o -- ask for a line while the sheet is enabled and the
    //     walk has not got there. Needs atm_en_o set, so run one good frame
    //     first.
    reset();
    write_bind(kBindPlaneAtm, kClutBase, kLStride, kLHeight);
    for (int i = 0; i < 256; ++i) write_pal(i, pal_entry(0, static_cast<uint8_t>(i)));
    int bad = 0, held = 0;
    run_frame(false, &bad, &held, 20000);
    top.frame_start_i = 1;
    zhao::tick(top);
    top.frame_start_i = 0;
    zhao::check(top.atm_en_o == 1, "the sheet is enabled for the second frame", 1, top.atm_en_o);
    const uint32_t un0 = top.atm_underrun_o;
    b.cx = 0;
    b.cy = H - 1;
    b.creq = true;
    for (int c = 0; c < 8; ++c) b.cycle();
    b.creq = false;
    zhao::check(top.atm_underrun_o > un0,
                "FIRED: atm_underrun_o sees the compositor ask for a line that is not ready", 1,
                top.atm_underrun_o > un0 ? 1 : 0);
  }

  const int rc = zhao::report_and_exit("twod_sampler_directed");
  zhao::exit_hard(rc);
}
