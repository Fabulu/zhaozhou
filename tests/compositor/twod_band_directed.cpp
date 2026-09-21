// twod_band_directed.cpp -- does the HUD band put descriptor-order pixels at the
// right raster address, and does R235's admission law actually DISCRIMINATE?
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS FOR
// ---------------------------------------------------------------------------
// `zhao_console_core.sv` entry I17: "TWOD.SPRITE walks in DESCRIPTOR order, one
// whole sprite at a time; `hud_*` is a random access in RASTER order." The band
// is the bridge, chosen over a frame store and a line ring by owner ruling R233.
//
// ---------------------------------------------------------------------------
// THE CASE THAT MATTERS MOST IS CASE 4, AND IT IS NOT A PIXEL CASE
// ---------------------------------------------------------------------------
// R95: a counter must DISCRIMINATE, not merely move. A refusal counter that
// fires on everything is as useless as one that fires on nothing, and a bench
// that only ever over-subscribes cannot tell the two apart. So case 4 builds a
// list that fits the bucket EXACTLY and requires ZERO refusals, then widens one
// sprite by a single pixel and requires EXACTLY ONE -- and then checks that the
// refused sprite drew nothing IN EVERY BAND, which is the half that makes
// "refuse the sprite WHOLE" true rather than merely counted.
//
// ---------------------------------------------------------------------------
// EVERY COUNTER IS FIRED HERE, BY LEGAL STIMULUS AT THIS BLOCK'S OWN PORTS
// ---------------------------------------------------------------------------
// Every neighbour of the band is bench-driven in this file, so states that are
// structurally unreachable in the composed console -- a starved sampler, a read
// address that leaves the sweep, a colour for a row outside the open band -- are
// ordinary stimulus here. That is the same shape `post_gather_store_directed.cpp`
// uses, and it is why the console smoke can assert those counters at zero and
// have the zero mean something.
//
// The one control that is NOT here is `zhao_twod_band_burst_mutant.sv`: raising
// `BURST_PX` past the FIFO slack is "admit everything", and its driver passes
// when `band_underrun_o` FIRES. That is evidence about the LAW -- that the
// admission test is what holds the guarantee up -- rather than about a counter.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_twod_band.h"

#include "zhao_sim.hpp"

namespace {

// The module's own defaults, restated so a change to either side is a test
// failure rather than a silent disagreement.
constexpr int kLineW = 384;
constexpr int kB     = 4;
constexpr int kL     = 16;
constexpr int kBurst = (kL - kB) * kLineW;   // 4608
constexpr int kMaxDesc = 64;

// A SHORT frame, not a NARROW one. The bucket's drain rate is `LINE_W` pixels
// per scanned LINE, so a bench that narrows the line silently halves the drain
// and every admission number in R233 stops meaning what it says. Height is free
// to shrink; width is not.
constexpr int kW = kLineW;
constexpr int kH = 32;

Vzhao_twod_band* top = nullptr;

// --------------------------------------------------------------------------
// A behavioural `zhao_twod_sprite` + `zhao_twod_sampler`: it takes the band's
// clipped slice and walks it one pixel per clock, stepping the row origin by
// (a01, a11) exactly as the real walker does. The colour it returns encodes the
// SPRITE and the CURSOR -- `(src << 8) | (u >> 16)` -- so a band that places a
// sprite correctly but hands it the wrong row origin still fails.
// --------------------------------------------------------------------------
struct Walk {
  bool     busy = false;
  int      x = 0, y = 0, w = 0, h = 0, px = 0, py = 0;
  uint32_t u = 0, v = 0, ru = 0, rv = 0, a00 = 0, a01 = 0, a10 = 0, a11 = 0;
  uint16_t tint = 0xFFFF, src = 0;
  uint8_t  ord = 0, blend = 0;
};
Walk W;

int  gStallEvery = 0;   // >0: the sampler refuses to produce on 1 of N cycles
int  gStallPhase = 0;
long gCycles = 0;

void driveSampler() {
  bool produce = W.busy;
  if (gStallEvery > 0 && ((gCycles + gStallPhase) % gStallEvery) != 0) produce = false;

  top->e_ready_i = !W.busy;
  top->c_valid_i = produce ? 1 : 0;
  top->c_tint_i  = W.tint;
  top->c_blend_i = W.blend;
  top->c_order_i = W.ord;
  top->c_src_id_i = W.src;
  if (produce) {
    top->c_x_i   = static_cast<int16_t>(W.x + W.px);
    top->c_y_i   = static_cast<int16_t>(W.y + W.py);
    top->c_rgb_i = static_cast<uint16_t>(((W.src & 0xFF) << 8) | ((W.u >> 16) & 0xFF));
    top->c_last_i = ((W.px == W.w - 1) && (W.py == W.h - 1)) ? 1 : 0;
  } else {
    top->c_last_i = 0;
  }
}

void cyc() {
  driveSampler();
  top->eval();

  const bool eFire = top->e_valid_o && top->e_ready_i;
  const bool cFire = top->c_valid_i && top->c_ready_o;

  Walk nw;
  if (eFire) {
    nw.busy = true;
    nw.x   = static_cast<int16_t>(top->e_x_o);
    nw.y   = static_cast<int16_t>(top->e_y_o);
    nw.w   = top->e_w_o;
    nw.h   = top->e_h_o;
    nw.u   = top->e_u_o;   nw.ru = nw.u;
    nw.v   = top->e_v_o;   nw.rv = nw.v;
    nw.a00 = top->e_a00_o; nw.a01 = top->e_a01_o;
    nw.a10 = top->e_a10_o; nw.a11 = top->e_a11_o;
    nw.tint = top->e_tint_o;
    nw.blend = top->e_blend_o;
    nw.ord = top->e_order_o;
    nw.src = top->e_src_id_o;
  }

  top->clk = 1; top->eval();
  top->clk = 0; top->eval();
  ++gCycles;

  if (cFire) {
    if (W.px == W.w - 1) {
      W.px = 0;
      ++W.py;
      W.ru += W.a01;  W.rv += W.a11;
      W.u = W.ru;     W.v = W.rv;
      if (W.py >= W.h) W.busy = false;
    } else {
      ++W.px;
      W.u += W.a00;  W.v += W.a10;
    }
  }
  if (eFire) W = nw;
}

void idleInputs() {
  top->d_valid_i = 0;
  top->rd_req_v_i = 0;
  top->rd_x_i = 0;
  top->rd_y_i = 0;
  top->frame_start_i = 0;
  top->frame_w_i = kW;
  top->frame_h_i = kH;
  top->view_split_i = 0;
}

// A FRESH DEVICE, NOT A SOFT RESET, AND THE DIFFERENCE IS A REAL PROPERTY OF
// THE DESIGN. `rst_n` clears the per-row generation counters; it does NOT clear
// the band store, because nothing can clear 6,144 words in a reset. So after a
// soft reset the generation sequence restarts at 1 and words written before it
// -- which also carry low generations -- can read fresh. The console's reset is
// a CONFIGURATION reset and an M10K comes up zero, which never matches; a
// mid-session soft reset is the exposure, and it is declared in the contract
// rather than papered over here. Each case therefore starts from a configured
// device, which is what the console does.
void hardReset() {
  delete top;
  top = new Vzhao_twod_band;
  W = Walk();
  gStallEvery = 0;
  idleInputs();
  top->rst_n = 0;
  top->clk = 0;
  for (int i = 0; i < 4; ++i) { top->eval(); top->clk = 1; top->eval(); top->clk = 0; top->eval(); }
  top->rst_n = 1;
  for (int i = 0; i < 2; ++i) cyc();
}

struct Desc {
  int      x = 0, y = 0, w = 1, h = 1;
  uint32_t u = 0, v = 0, a00 = 0, a01 = 0x10000, a10 = 0, a11 = 0;
  int      fmt = 1, pal = 0, blend = 0, vm = 1, ord = 0;
  uint16_t tint = 0xFFFF, src = 0;
};

void pushDesc(const Desc& d) {
  top->d_valid_i = 1;
  top->d_x_i = static_cast<int16_t>(d.x);
  top->d_y_i = static_cast<int16_t>(d.y);
  top->d_w_i = static_cast<uint16_t>(d.w);
  top->d_h_i = static_cast<uint16_t>(d.h);
  top->d_u_i = d.u;
  top->d_v_i = d.v;
  top->d_a00_i = d.a00;
  top->d_a01_i = d.a01;
  top->d_a10_i = d.a10;
  top->d_a11_i = d.a11;
  top->d_format_i = d.fmt;
  top->d_palette_i = d.pal;
  top->d_tint_i = d.tint;
  top->d_blend_i = d.blend;
  top->d_view_mask_i = d.vm;
  top->d_order_i = d.ord;
  top->d_src_id_i = d.src;
  cyc();
  top->d_valid_i = 0;
}

void frameStart() {
  top->frame_start_i = 1;
  cyc();
  top->frame_start_i = 0;
}

// Let the filler run with no reader, the way the console's frame tick gives it
// the whole render interval before the compositor's pass begins.
void leadIn(int cycles = 200000) {
  // RELATIVE to where the counter is now. `bands_o` is a lifetime total and is
  // not reset by a frame tick, so an absolute target returns immediately on the
  // second frame and hands the filler no lead at all -- which then reads as an
  // underrun in the design rather than in the bench.
  const uint32_t want = top->bands_o + 4;   // SLOTS bands pre-filled
  for (int i = 0; i < cycles; ++i) {
    cyc();
    if (top->bands_o >= want) return;
  }
}

// Sweep the read port exactly as POST.COMPOSITE does, one pixel per clock in
// raster order, while the filler keeps working.
std::vector<int> sweep() {
  std::vector<int> fb(static_cast<size_t>(kW) * kH, -1);
  const int n = kW * kH;
  for (int i = 0; i <= n; ++i) {
    if (i < n) {
      top->rd_req_v_i = 1;
      top->rd_x_i = i % kW;
      top->rd_y_i = i / kW;
    } else {
      top->rd_req_v_i = 0;
    }
    cyc();
    if (i >= 1) fb[static_cast<size_t>(i - 1)] = top->rd_valid_o ? static_cast<int>(top->rd_rgb_o) : -1;
  }
  top->rd_req_v_i = 0;
  return fb;
}

int countLit(const std::vector<int>& fb) {
  int n = 0;
  for (int v : fb) if (v >= 0) ++n;
  return n;
}

int countSrc(const std::vector<int>& fb, int src) {
  int n = 0;
  for (int v : fb) if (v >= 0 && ((v >> 8) & 0xFF) == src) ++n;
  return n;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  top = new Vzhao_twod_band;

  uint32_t firedRefuse = 0, firedOob = 0, firedUnderrun = 0, firedMismatch = 0;
  uint32_t firedOverflow = 0, firedTint = 0, firedBlend = 0, firedClip = 0;
  uint32_t firedInversion = 0;

  // ======================================================================
  // case0: ONE SPRITE LANDS AT ITS SCREEN ADDRESS, AND NOWHERE ELSE.
  // ======================================================================
  {
    hardReset();
    frameStart();
    Desc d; d.x = 10; d.y = 6; d.w = 6; d.h = 3; d.src = 0x21; d.a01 = 0x10000;
    pushDesc(d);
    leadIn();
    std::vector<int> fb = sweep();

    zhao::check(countLit(fb) == 18, "a 6x3 sprite lights exactly 18 HUD pixels",
                18, static_cast<uint64_t>(countLit(fb)));
    bool placed = true, cursor = true;
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 6; ++col) {
        const int v = fb[static_cast<size_t>((6 + row) * kW + 10 + col)];
        if (v < 0) placed = false;
        else if (v != ((0x21 << 8) | row)) cursor = false;
      }
    }
    zhao::check(placed, "every pixel of the sprite is at its screen address", 1, placed ? 1 : 0);
    // THE CURSOR IS THE POINT. `a01` steps u by one unit per ROW, so the colour
    // encodes which row of the SPRITE the band believed it was emitting. A band
    // that restarts the cursor at every band boundary fails here and nowhere
    // else -- and this sprite straddles one, at y = 8.
    zhao::check(cursor, "the row origin is right in every row, ACROSS a band boundary",
                1, cursor ? 1 : 0);
    zhao::check(top->sprites_admitted_o == 1, "one sprite admitted", 1, top->sprites_admitted_o);
    zhao::check(top->sprites_refused_budget_o == 0, "nothing refused", 0,
                top->sprites_refused_budget_o);
    zhao::check(top->band_underrun_o == 0,
                "the filler's lead-in means the reader never outruns it", 0,
                top->band_underrun_o);
    zhao::check(top->scan_addr_mismatch_o == 0,
                "a monotonic sweep produces no address mismatch", 0,
                top->scan_addr_mismatch_o);
    zhao::check(top->write_oob_o == 0, "no colour landed outside the open band", 0,
                top->write_oob_o);
  }

  // ======================================================================
  // case1: A SPRITE THAT SPANS MANY BANDS IS DRAWN WHOLE, ROW FOR ROW.
  // ======================================================================
  {
    hardReset();
    frameStart();
    Desc d; d.x = 2; d.y = 1; d.w = 5; d.h = 20; d.src = 0x33; d.a01 = 0x10000;
    pushDesc(d);
    leadIn();
    std::vector<int> fb = sweep();
    int ok = 0;
    for (int row = 0; row < 20; ++row)
      for (int col = 0; col < 5; ++col)
        if (fb[static_cast<size_t>((1 + row) * kW + 2 + col)] == ((0x33 << 8) | row)) ++ok;
    zhao::check(ok == 100, "a 20-row sprite crosses five bands with an exact cursor",
                100, static_cast<uint64_t>(ok));
    zhao::check(countLit(fb) == 100, "and lights nothing else", 100,
                static_cast<uint64_t>(countLit(fb)));
    zhao::check(top->slices_emitted_o >= 5,
                "it was emitted as one band-clipped slice per band it touches", 5,
                top->slices_emitted_o);
  }

  // ======================================================================
  // case2: THE GENERATION LAW. A second frame with no descriptors must show
  // NOTHING -- no clear cycle is ever spent, so this is the only thing standing
  // between the reader and last frame's pixels.
  // ======================================================================
  {
    hardReset();
    frameStart();
    Desc d; d.x = 3; d.y = 2; d.w = 8; d.h = 12; d.src = 0x55;
    pushDesc(d);
    leadIn();
    std::vector<int> fb1 = sweep();
    zhao::check(countLit(fb1) == 96, "frame 1 draws the sprite", 96,
                static_cast<uint64_t>(countLit(fb1)));

    frameStart();      // frame 2: the list is cleared, nothing is pushed
    leadIn();
    std::vector<int> fb2 = sweep();
    zhao::check(countLit(fb2) == 0,
                "frame 2 shows NOTHING of frame 1 -- the per-row generation is "
                "bumped when a band opens, and no word is ever cleared",
                0, static_cast<uint64_t>(countLit(fb2)));
  }

  // ======================================================================
  // case3: EDGE CLIPPING, ALL FOUR SIDES.
  // ======================================================================
  {
    hardReset();
    frameStart();
    Desc a; a.x = -3; a.y = 4;  a.w = 6; a.h = 2; a.src = 0x11;   // off the left
    Desc b; b.x = kW - 3; b.y = 8; b.w = 6; b.h = 2; b.src = 0x12; // off the right
    Desc c; c.x = 20; c.y = -3; c.w = 4; c.h = 6; c.src = 0x13;    // off the top
    Desc e; e.x = 30; e.y = kH - 3; e.w = 4; e.h = 6; e.src = 0x14; // off the bottom
    pushDesc(a); pushDesc(b); pushDesc(c); pushDesc(e);
    leadIn();
    std::vector<int> fb = sweep();
    zhao::check(countSrc(fb, 0x11) == 6, "left edge: 3 of 6 columns survive x2 rows", 6,
                static_cast<uint64_t>(countSrc(fb, 0x11)));
    zhao::check(countSrc(fb, 0x12) == 6, "right edge: 3 columns survive x2 rows", 6,
                static_cast<uint64_t>(countSrc(fb, 0x12)));
    zhao::check(countSrc(fb, 0x13) == 12, "top edge: 3 of 6 rows survive x4 cols", 12,
                static_cast<uint64_t>(countSrc(fb, 0x13)));
    zhao::check(countSrc(fb, 0x14) == 12, "bottom edge: 3 of 6 rows survive x4 cols", 12,
                static_cast<uint64_t>(countSrc(fb, 0x14)));
    // THE TOP-CLIPPED SPRITE IS THE CURSOR FAST-FORWARD CASE: its first live row
    // is row 3 of the sprite, so the serial shift-add must have run.
    zhao::check(fb[static_cast<size_t>(0 * kW + 20)] == ((0x13 << 8) | 3),
                "a top-clipped sprite's first drawn row carries the FAST-FORWARDED "
                "cursor, not its own u0",
                static_cast<uint64_t>((0x13 << 8) | 3),
                static_cast<uint64_t>(fb[static_cast<size_t>(0 * kW + 20)]));
    zhao::check(top->pixels_clipped_o > 0, "off-screen columns are dropped and COUNTED",
                1, top->pixels_clipped_o > 0 ? 1 : 0);
    firedClip = top->pixels_clipped_o;
    zhao::check(top->sprites_refused_budget_o == 0, "and none of this is a refusal", 0,
                top->sprites_refused_budget_o);
  }

  // ======================================================================
  // case4: R235 -- AND IT DISCRIMINATES.
  //
  // Pack the bucket to exactly `kBurst` and require ZERO refusals; then repeat
  // with one sprite one pixel wider and require EXACTLY ONE -- and require that
  // the refused sprite drew nothing in ANY band.
  //
  // The arithmetic, using the block's own marginal-excess rule:
  //   sprite 0: w = kW (64), rows 24  -> new_rate 64 <= 384, excess 0, need 0
  //   sprites 1..N: w = 384 each, rows 24
  //       the first pushes new_rate past 384 partway, the rest charge w * rows
  //   The list below is built so the total `need` lands exactly on kBurst.
  // ======================================================================
  {
    // Two full-width sprites: the first sits AT rate (excess 0), the second is
    // wholly excess: 384 * 12 = 4608 = kBurst exactly.
    hardReset();
    frameStart();
    Desc bar;  bar.x = 0; bar.y = 0; bar.w = kLineW; bar.h = 12; bar.src = 0x40;
    Desc over; over.x = 0; over.y = 0; over.w = kLineW; over.h = 12; over.src = 0x41;
    pushDesc(bar); pushDesc(over);
    leadIn();
    sweep();
    zhao::check(top->sprites_refused_budget_o == 0,
                "a list that lands EXACTLY on the bucket refuses nothing -- 384*12 "
                "= 4608 = (L-B)*LINE_W",
                0, top->sprites_refused_budget_o);
    zhao::check(top->sprites_admitted_o == 2, "both were admitted", 2,
                top->sprites_admitted_o);

    // One pixel more of height on the second sprite: need = 384 * 13 = 4992 > 4608.
    hardReset();
    frameStart();
    Desc over2 = over; over2.h = 13;
    pushDesc(bar); pushDesc(over2);
    leadIn();
    std::vector<int> fb = sweep();
    zhao::check(top->sprites_refused_budget_o == 1,
                "ONE more row of the same sprite refuses EXACTLY ONE sprite -- the "
                "counter discriminates, it does not merely move",
                1, top->sprites_refused_budget_o);
    zhao::check(top->sprites_admitted_o == 1, "and exactly one was admitted", 1,
                top->sprites_admitted_o);
    firedRefuse = top->sprites_refused_budget_o;

    // THE HALF THAT MAKES "WHOLE" TRUE. The refused sprite spans four bands; a
    // per-band admission test would have drawn it in some of them.
    zhao::check(countSrc(fb, 0x41) == 0,
                "the REFUSED sprite drew ZERO pixels, in every band it spans -- "
                "TWOD.SPRITE.md: never a partial sprite",
                0, static_cast<uint64_t>(countSrc(fb, 0x41)));
    zhao::check(countSrc(fb, 0x40) > 0, "while the admitted one drew normally", 1,
                countSrc(fb, 0x40) > 0 ? 1 : 0);
  }

  // ======================================================================
  // case5: R233's WORKED HUD -- a full-width status bar with glyphs over it.
  // The ruling's own numbers: the bar "sits exactly at rate", and 40 glyphs of
  // 8x12 need "ten lines of burst" = 3,840 inside 4,608.
  // ======================================================================
  {
    hardReset();
    frameStart();
    Desc bar; bar.x = 0; bar.y = 0; bar.w = kLineW; bar.h = 32; bar.src = 0x60;
    pushDesc(bar);
    for (int g = 0; g < 40; ++g) {
      Desc gl; gl.x = (g % 8) * 8; gl.y = 8 + (g % 3); gl.w = 8; gl.h = 12;
      gl.src = static_cast<uint16_t>(0x80 + g);
      pushDesc(gl);
    }
    leadIn();
    sweep();
    zhao::check(top->sprites_refused_budget_o == 0,
                "R233's worked HUD -- a full-width bar AT RATE plus 40 glyphs "
                "needing 3,840 of 4,608 burst -- is admitted whole",
                0, top->sprites_refused_budget_o);
    zhao::check(top->sprites_admitted_o == 41, "all 41 descriptors were admitted", 41,
                top->sprites_admitted_o);
    zhao::check(top->band_underrun_o == 0,
                "and the store never fell behind the sweep while drawing it", 0,
                top->band_underrun_o);
  }

  // ======================================================================
  // case6: THE DISPLAY LIST IS FULL. Also a whole-sprite refusal, also counted,
  // and also decided before one pixel is rasterised.
  // ======================================================================
  {
    hardReset();
    frameStart();
    for (int i = 0; i < kMaxDesc + 3; ++i) {
      Desc d; d.x = 0; d.y = 0; d.w = 1; d.h = 1; d.src = static_cast<uint16_t>(i);
      pushDesc(d);
    }
    zhao::check(top->descriptors_o == kMaxDesc, "the list accepts exactly MAX_DESC",
                kMaxDesc, top->descriptors_o);
    zhao::check(top->desc_overflow_o == 3,
                "and the three that did not fit are refused WHOLE and counted -- "
                "once per offer, not once per clock the offer is held",
                3, top->desc_overflow_o);
    firedOverflow = top->desc_overflow_o;
  }

  // ======================================================================
  // case7: THE READ ADDRESS LEAVES THE SWEEP. The instrument fires, and the
  // answer still comes from (x, y) -- the counter is the counter, never the
  // address.
  // ======================================================================
  {
    hardReset();
    frameStart();
    Desc d; d.x = 4; d.y = 4; d.w = 4; d.h = 4; d.src = 0x77;
    pushDesc(d);
    leadIn();

    // Read (5,5) directly, out of sweep order.
    top->rd_req_v_i = 1; top->rd_x_i = 5; top->rd_y_i = 5;
    cyc(); cyc();
    const int got = top->rd_valid_o ? static_cast<int>(top->rd_rgb_o) : -1;
    top->rd_req_v_i = 0; cyc();
    zhao::check(got == ((0x77 << 8) | 1),
                "an out-of-sweep read is still answered from its own (x, y)",
                static_cast<uint64_t>((0x77 << 8) | 1), static_cast<uint64_t>(got));
    zhao::check(top->scan_addr_mismatch_o > 0,
                "and the sweep instrument FIRES -- R233 records hud_req_* as a "
                "monotonic sweep, and this is what says so in silicon",
                1, top->scan_addr_mismatch_o > 0 ? 1 : 0);
    firedMismatch = top->scan_addr_mismatch_o;
  }

  // ======================================================================
  // case8: A COLOUR FOR A ROW OUTSIDE THE OPEN BAND. Structurally unreachable
  // in the composed console, because the band itself chose the rows; reachable
  // here because the sampler is this bench.
  // ======================================================================
  {
    hardReset();
    frameStart();
    Desc d; d.x = 0; d.y = 0; d.w = 2; d.h = 2; d.src = 0x08;
    pushDesc(d);
    for (int i = 0; i < 40; ++i) cyc();

    // Forge a colour for a row far outside anything the band has open.
    W = Walk();
    top->c_valid_i = 1;
    top->c_x_i = 1;
    top->c_y_i = 200;
    top->c_rgb_i = 0x1234;
    top->c_tint_i = 0xFFFF;
    top->c_blend_i = 0;
    top->c_order_i = 0;
    top->c_src_id_i = 0x99;
    top->c_last_i = 0;
    top->e_ready_i = 0;
    top->eval();
    top->clk = 1; top->eval(); top->clk = 0; top->eval();
    ++gCycles;
    top->c_valid_i = 0;
    cyc();
    zhao::check(top->write_oob_o >= 1,
                "a colour for a row outside the open band is DROPPED and counted",
                1, top->write_oob_o >= 1 ? 1 : 0);
    firedOob = top->write_oob_o;
  }

  // ======================================================================
  // case9: TINT AND BLEND ARE CARRIED, NOT APPLIED, AND THE SIZE OF THAT GAP
  // IS A NUMBER. `zhao_twod_sampler` already ships `tint_unapplied_o` per
  // SAMPLE; this is the same statement per HUD PIXEL THAT REACHED THE SCREEN.
  // ======================================================================
  {
    hardReset();
    frameStart();
    Desc d; d.x = 0; d.y = 0; d.w = 4; d.h = 4; d.src = 0x0A;
    d.tint = 0x7BEF; d.blend = 2;
    pushDesc(d);
    leadIn();
    sweep();
    zhao::check(top->tint_dropped_o == 16, "16 tinted pixels carried an unapplied tint",
                16, top->tint_dropped_o);
    zhao::check(top->blend_dropped_o == 16, "and an unapplied blend mode", 16,
                top->blend_dropped_o);
    firedTint = top->tint_dropped_o;
    firedBlend = top->blend_dropped_o;
  }

  // ======================================================================
  // case10: THE ORDER LAW. The band composites by LAST WRITE, so
  // TWOD.SPRITE.md's "composited output order equals descriptor order" holds
  // exactly while the list is in order -- and this counter says whether it was.
  // ======================================================================
  {
    hardReset();
    frameStart();
    Desc lo; lo.x = 0; lo.y = 0; lo.w = 4; lo.h = 4; lo.src = 0x01; lo.ord = 200;
    Desc hi; hi.x = 0; hi.y = 0; hi.w = 4; hi.h = 4; hi.src = 0x02; hi.ord = 5;
    pushDesc(lo); pushDesc(hi);
    leadIn();
    std::vector<int> fb = sweep();
    zhao::check(top->order_inversion_o > 0,
                "a display list OUT of `order` is counted, not silently obeyed",
                1, top->order_inversion_o > 0 ? 1 : 0);
    firedInversion = top->order_inversion_o;
    zhao::check(((fb[0] >> 8) & 0xFF) == 0x02,
                "and the band composites by LAST WRITE, which is list order",
                0x02, static_cast<uint64_t>((fb[0] >> 8) & 0xFF));
  }

  // ======================================================================
  // case11: A STARVED SAMPLER. The band cannot close a band whose pixels have
  // not arrived, and the reader cannot be told to wait -- so the guard fires.
  // In the composed console the sampler runs at one pixel per clock and the
  // admission law bounds the deficit, which is why the smoke asserts this at
  // zero; THIS is what makes that zero evidence.
  // ======================================================================
  {
    hardReset();
    frameStart();
    Desc d; d.x = 0; d.y = 0; d.w = kW; d.h = kH; d.src = 0x0C;
    pushDesc(d);
    gStallEvery = 64;          // the sampler produces on 1 cycle in 64
    for (int i = 0; i < 200; ++i) cyc();
    sweep();
    zhao::check(top->band_underrun_o > 0,
                "a sampler starved to 1/64 of rate makes the reader outrun the "
                "filler, and `band_underrun_o` FIRES",
                1, top->band_underrun_o > 0 ? 1 : 0);
    firedUnderrun = top->band_underrun_o;
    gStallEvery = 0;
  }

  // The counters below are NOT this block's live state -- the cases reset it
  // repeatedly, so a live read prints zero and reads exactly like a bench that
  // never fired them. Captured at the moment each one moved instead.
  std::printf(
      "  POSITIVE CONTROLS FIRED: refused_budget=%u desc_overflow=%u "
      "write_oob=%u band_underrun=%u scan_addr_mismatch=%u\n"
      "                           pixels_clipped=%u tint_dropped=%u "
      "blend_dropped=%u order_inversion=%u\n"
      "  The composed console asserts refused_budget / write_oob / "
      "band_underrun / scan_addr_mismatch / order_inversion at ZERO, and these "
      "are the firings that make those zeros evidence rather than silence.\n",
      firedRefuse, firedOverflow, firedOob, firedUnderrun, firedMismatch,
      firedClip, firedTint, firedBlend, firedInversion);

  return zhao::report_and_exit("twod_band_directed");
}
