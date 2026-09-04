// post_gather_random.cpp — a randomized tile against the accumulation law.
//
// ---------------------------------------------------------------------------
// WHAT RANDOM BUYS HERE THAT DIRECTED DOES NOT
// ---------------------------------------------------------------------------
// The directed lane checks the named properties one at a time on clean inputs.
// What it cannot check is the ACCUMULATION over a whole tile's worth of
// fragments landing in an unpredictable order: sixteen cells, three saturating
// glow channels, two saturating signed displacement lanes and an OR, all
// updated by hundreds of fragments whose only pattern is that there isn't one.
//
// The model is `zref::post::*` — the same law, in C++, applied in the same
// order. It is not a transcription of the RTL, so the two agreeing is evidence
// rather than a tautology.
//
// The saturation cases are what this is really for. A glow channel driven past
// 65,535 and a displacement driven past the s16 lane are both reachable in one
// tile of a bright frame, and both fail SILENTLY: the picture is merely a bit
// dimmer, or a bit less displaced, than it should be.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_post_gather.h"

#include "zhao_sim.hpp"
#include "zref/zref_post.hpp"

namespace {

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

struct ModelCell {
  uint16_t r = 0, g = 0, b = 0;
  int32_t dx = 0, dy = 0;  // the RTL's wide saturating s16 lane
  bool ink = false;
};

int32_t sat_s16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return v;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_post_gather top;

  auto idle = [&]() {
    top.f_valid_i = 0;
    top.tile_start_i = 0;
    top.tile_flush_i = 0;
  };
  idle();
  top.rst_n = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  uint32_t s = 0x0FF1CEu;
  int bad_glow = 0, bad_dx = 0, bad_dy = 0, bad_ink = 0;
  int tiles = 0, saturated_channels = 0, clamped_cells = 0;

  for (int tile = 0; tile < 24; ++tile) {
    std::vector<ModelCell> m(16);

    top.tile_start_i = 1;
    zhao::tick(top);
    top.tile_start_i = 0;

    // EVERY THIRD TILE IS A CONCENTRATED ONE. The first version spread 200-600
    // fragments over sixteen cells at random brightness -- about 37 per cell,
    // peaking near 9,500 against a u16 accumulator, so NOTHING EVER SATURATED.
    // The lane agreed with the model perfectly and the coverage guard caught
    // it: "0 saturated channels" read as agreement when the stimulus never
    // asked the question.
    //
    // A u16 channel needs 257 maximum contributions to saturate, so a
    // saturating tile has to aim them at one cell.
    const bool concentrated = (tile % 3) == 0;
    const int n = concentrated ? 1200 : 200 + static_cast<int>(rnd(&s) % 400u);
    for (int i = 0; i < n; ++i) {
      const int x = concentrated ? 1 : static_cast<int>(rnd(&s) & 15u);
      const int y = concentrated ? 1 : static_cast<int>(rnd(&s) & 15u);
      const int r = concentrated ? 255 : static_cast<int>(rnd(&s) & 0xFFu);
      const int g = concentrated ? 255 : static_cast<int>(rnd(&s) & 0xFFu);
      const int b = concentrated ? 200 : static_cast<int>(rnd(&s) & 0xFFu);
      const int16_t dx = static_cast<int16_t>(rnd(&s) & 0xFFFFu);
      const int16_t dy = static_cast<int16_t>(rnd(&s) & 0xFFFFu);
      const bool ink = (rnd(&s) & 7u) == 0u;

      top.f_valid_i = 1;
      top.f_x_i = x;
      top.f_y_i = y;
      top.f_glow_r_i = r;
      top.f_glow_g_i = g;
      top.f_glow_b_i = b;
      top.f_disp_x_i = static_cast<uint16_t>(dx);
      top.f_disp_y_i = static_cast<uint16_t>(dy);
      top.f_ink_i = ink ? 1 : 0;
      zhao::tick(top);
      top.f_valid_i = 0;

      // the model, in the same order
      const int cell = ((y >> 2) << 2) | (x >> 2);
      ModelCell& c = m[cell];
      c.r = zref::post::glow_accumulate(c.r, static_cast<uint8_t>(r));
      c.g = zref::post::glow_accumulate(c.g, static_cast<uint8_t>(g));
      c.b = zref::post::glow_accumulate(c.b, static_cast<uint8_t>(b));
      c.dx = sat_s16(c.dx + dx);
      c.dy = sat_s16(c.dy + dy);
      c.ink = c.ink || ink;
    }

    // swap so this tile becomes flushable, then read it back
    top.tile_start_i = 1;
    zhao::tick(top);
    top.tile_start_i = 0;
    top.tile_flush_i = 1;
    zhao::tick(top);
    top.tile_flush_i = 0;

    for (int c = 0; c < 40; ++c) {
      top.eval();
      if (top.c_valid_o) {
        const int idx = top.c_index_o;
        const ModelCell& want = m[idx];
        if (top.c_glow_o != zref::post::glow_pack565(want.r, want.g, want.b)) ++bad_glow;
        bool cx = false, cy = false;
        const int wx =
            zref::post::disp_to_pixels(static_cast<int16_t>(want.dx), zref::post::kDispMaxX, &cx);
        const int wy =
            zref::post::disp_to_pixels(static_cast<int16_t>(want.dy), zref::post::kDispMaxY, &cy);
        if (static_cast<int8_t>(top.c_disp_x_o) != wx) ++bad_dx;
        if (static_cast<int8_t>(top.c_disp_y_o) != wy) ++bad_dy;
        if ((top.c_ink_o != 0) != want.ink) ++bad_ink;
        if (cx || cy) ++clamped_cells;
        if (want.r == 0xFFFF || want.g == 0xFFFF || want.b == 0xFFFF) ++saturated_channels;
      }
      zhao::tick(top);
    }
    ++tiles;
  }

  zhao::check(bad_glow == 0,
              "every cell's packed glow matches zref::post::glow_pack565 across "
              "24 randomized tiles",
              0, bad_glow);
  zhao::check(bad_dx == 0 && bad_dy == 0,
              "and every displacement matches zref::post::disp_to_pixels, "
              "including the ones that saturated the accumulator before they "
              "were clamped",
              0, bad_dx + bad_dy);
  zhao::check(bad_ink == 0, "and the ink bit ORs correctly", 0, bad_ink);

  // The traversal has to have REACHED the cases it exists for. A run with no
  // saturation and no clamping would agree with the model perfectly and prove
  // nothing about either.
  zhao::check(saturated_channels > 0,
              "the tiles were bright enough to SATURATE a glow channel -- "
              "otherwise this agrees with the model about a case it never hit",
              1, saturated_channels > 0 ? 1 : 0);
  zhao::check(clamped_cells > 0, "and to clamp a displacement", 1, clamped_cells > 0 ? 1 : 0);

  std::printf("  %d tiles, %d saturated channels, %d clamped cells\n", tiles, saturated_channels,
              clamped_cells);

  return zhao::report_and_exit("post_gather_random");
}
