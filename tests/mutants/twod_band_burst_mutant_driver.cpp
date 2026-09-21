// twod_band_burst_mutant_driver.cpp -- INVERTED POLARITY. It passes when
// `band_underrun_o` FIRES.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS EVIDENCE ABOUT
// ---------------------------------------------------------------------------
// `tests/compositor/twod_band_directed.cpp` already fires `band_underrun_o` by
// starving the sampler. That proves the INSTRUMENT works. It does not prove the
// LAW is load-bearing -- a starved sampler is a fault on a different port, and
// "the admission test is what keeps the filler inside the FIFO slack" stays an
// argument.
//
// This is the other half. The sampler here is PERFECTLY HEALTHY: one pixel per
// clock, never stalling. The only thing that changes is `BURST_PX`, raised past
// the FIFO slack in `zhao_twod_band_burst_mutant.sv`, which is "admit
// everything". Six full-width sprites are offered; production refuses five of
// them and draws a HUD, and the mutant admits all six and falls behind the
// sweep.
//
// THE PLANT IS PROVEN BEFORE THE FIRING IS QUOTED. `sprites_refused_budget_o`
// must read ZERO here: the same stimulus against production refuses five, so a
// zero says the parameter override actually engaged. A fire test whose mutation
// silently did not apply reports a reassuring result about nothing, which is
// this repository's most repeated shape.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_twod_band_burst_mutant.h"

#include "zhao_sim.hpp"

namespace {

constexpr int kW = 384;   // a full line: the bucket drains LINE_W per LINE
constexpr int kH = 32;

Vzhao_twod_band_burst_mutant* top = nullptr;

struct Walk {
  bool     busy = false;
  int      x = 0, y = 0, w = 0, h = 0, px = 0, py = 0;
  uint32_t u = 0, ru = 0, a00 = 0, a01 = 0, a10 = 0, a11 = 0;
  uint16_t src = 0;
};
Walk W;

void cyc() {
  top->e_ready_i  = !W.busy;
  top->c_valid_i  = W.busy ? 1 : 0;
  top->c_tint_i   = 0xFFFF;
  top->c_blend_i  = 0;
  top->c_order_i  = 0;
  top->c_src_id_i = W.src;
  if (W.busy) {
    top->c_x_i    = static_cast<int16_t>(W.x + W.px);
    top->c_y_i    = static_cast<int16_t>(W.y + W.py);
    top->c_rgb_i  = static_cast<uint16_t>(((W.src & 0xFF) << 8) | ((W.u >> 16) & 0xFF));
    top->c_last_i = ((W.px == W.w - 1) && (W.py == W.h - 1)) ? 1 : 0;
  } else {
    top->c_last_i = 0;
  }
  top->eval();

  const bool eFire = top->e_valid_o && top->e_ready_i;
  const bool cFire = top->c_valid_i && top->c_ready_o;

  Walk nw;
  if (eFire) {
    nw.busy = true;
    nw.x = static_cast<int16_t>(top->e_x_o);
    nw.y = static_cast<int16_t>(top->e_y_o);
    nw.w = top->e_w_o;
    nw.h = top->e_h_o;
    nw.u = top->e_u_o; nw.ru = nw.u;
    nw.a00 = top->e_a00_o; nw.a01 = top->e_a01_o;
    nw.a10 = top->e_a10_o; nw.a11 = top->e_a11_o;
    nw.src = top->e_src_id_o;
  }

  top->clk = 1; top->eval();
  top->clk = 0; top->eval();

  if (cFire) {
    if (W.px == W.w - 1) {
      W.px = 0; ++W.py; W.ru += W.a01; W.u = W.ru;
      if (W.py >= W.h) W.busy = false;
    } else {
      ++W.px; W.u += W.a00;
    }
  }
  if (eFire) W = nw;
}

void pushDesc(int x, int y, int w, int h, uint16_t src) {
  top->d_valid_i = 1;
  top->d_x_i = static_cast<int16_t>(x);
  top->d_y_i = static_cast<int16_t>(y);
  top->d_w_i = static_cast<uint16_t>(w);
  top->d_h_i = static_cast<uint16_t>(h);
  top->d_u_i = 0;
  top->d_v_i = 0;
  top->d_a00_i = 0;
  top->d_a01_i = 0x10000;
  top->d_a10_i = 0;
  top->d_a11_i = 0;
  top->d_format_i = 1;
  top->d_palette_i = 0;
  top->d_tint_i = 0xFFFF;
  top->d_blend_i = 0;
  top->d_view_mask_i = 1;
  top->d_order_i = 0;
  top->d_src_id_i = src;
  cyc();
  top->d_valid_i = 0;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  top = new Vzhao_twod_band_burst_mutant;

  top->d_valid_i = 0;
  top->rd_req_v_i = 0;
  top->rd_x_i = 0;
  top->rd_y_i = 0;
  top->frame_start_i = 0;
  top->frame_w_i = kW;
  top->frame_h_i = kH;
  top->view_split_i = 0;
  top->rst_n = 0;
  top->clk = 0;
  for (int i = 0; i < 4; ++i) { top->eval(); top->clk = 1; top->eval(); top->clk = 0; top->eval(); }
  top->rst_n = 1;
  for (int i = 0; i < 2; ++i) cyc();

  top->frame_start_i = 1; cyc(); top->frame_start_i = 0;

  // Six full-width, full-height sprites: 6 x 384 x 32 = 73,728 pixels of work
  // against a 12,288-clock sweep. Production admits the first and refuses five.
  for (int i = 0; i < 6; ++i) pushDesc(0, 0, kW, kH, static_cast<uint16_t>(0xA0 + i));

  // The same lead-in the console's frame tick gives the filler.
  const uint32_t want = top->bands_o + 4;
  for (int i = 0; i < 400000; ++i) { cyc(); if (top->bands_o >= want) break; }

  const int n = kW * kH;
  for (int i = 0; i <= n; ++i) {
    if (i < n) { top->rd_req_v_i = 1; top->rd_x_i = i % kW; top->rd_y_i = i / kW; }
    else       { top->rd_req_v_i = 0; }
    cyc();
  }
  top->rd_req_v_i = 0;

  // THE PLANT, PROVEN FIRST.
  zhao::check(top->sprites_refused_budget_o == 0,
              "MUTANT: BURST_PX = 1,000,000 admits everything, so the refusal "
              "counter cannot move -- production refuses five of these six, so a "
              "zero here is the parameter override engaging",
              0, top->sprites_refused_budget_o);
  zhao::check(top->sprites_admitted_o == 6,
              "MUTANT: all six full-width sprites were admitted", 6,
              top->sprites_admitted_o);

  // THE FIRING.
  zhao::check(top->band_underrun_o > 0,
              "MUTANT (INVERTED POLARITY -- this PASSES when it fires): with the "
              "admission law effectively removed and a PERFECTLY HEALTHY sampler, "
              "the filler falls behind the sweep and `band_underrun_o` moves. "
              "R235's refusal is what holds that guarantee up.",
              1, top->band_underrun_o > 0 ? 1 : 0);

  std::printf("  MUTANT FIRED: band_underrun=%u  admitted=%u  refused=%u\n",
              top->band_underrun_o, top->sprites_admitted_o,
              top->sprites_refused_budget_o);

  return zhao::report_and_exit("twod_band_burst_mutant");
}
