// raster_attrinterp_directed.cpp — does the stepped plane land on the pixel
// CENTRES the oracle actually samples?
//
// ---------------------------------------------------------------------------
// WHAT IS AT RISK
// ---------------------------------------------------------------------------
// The plane form is already proved equivalent to the oracle's per-pixel
// numerator (tests/proofs/attribute_plane_equivalence.cpp), and the divide is
// already proved exact. What is left to get wrong is WHERE the plane is
// evaluated, and there is exactly one way to get it wrong that nothing else
// catches:
//
//   THE HALF PIXEL. spec §8 puts the centre of pixel p at 256*p + 128
//   subpixels, and rast.cpp and RASTER.EDGEWALK both sample there. A block that
//   evaluated the plane at pixel CORNERS would interpolate just as smoothly,
//   with exactly the right gradient, and every attribute would be off by half a
//   step in each axis. Flat gradients hide it completely; steep ones move every
//   golden capture CRC.
//
// So this file never compares against a re-derivation of the plane. It compares
// against `orient()` evaluated at 256*p + 128 and combined with the vertex
// attributes -- the oracle's own arithmetic at the oracle's own sample point. A
// missing +128 fails section 1 on every triangle with a non-zero gradient, and
// section 4 exists to prove those gradients were non-zero in the first place.
//
// The other two risks are cheaper but real: the emitted (row, col) stream must
// be exactly the coverage mask in raster order with nothing dropped or
// duplicated, and `n_last_o` must fire exactly once, on the final covered pixel
// -- not on the final column, which is usually not covered.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_raster_attrinterp.h"

#include "zhao_sim.hpp"

namespace {

int64_t orient(int64_t ux, int64_t uy, int64_t vx, int64_t vy, int64_t px, int64_t py) {
  return (vx - ux) * (py - uy) - (vy - uy) * (px - ux);
}

/** The centre of pixel p, in S 12.8 subpixels (spec §8). */
int64_t centre(int64_t p) { return (p << 8) + 128; }

struct Tri {
  int64_t ax, ay, bx, by, cx, cy;
  int64_t va, vb, vc;
  const char* what;
};

struct Emit {
  int row, col;
  __int128 num;
  bool last;
};

void put_wide(uint32_t* w, int words, __int128 v) {
  for (int i = 0; i < words; ++i) w[i] = static_cast<uint32_t>((v >> (32 * i)) & 0xFFFFFFFFu);
}

__int128 get_wide(const uint32_t* w, int bits) {
  __int128 v = 0;
  const int words = (bits + 31) / 32;
  for (int i = words - 1; i >= 0; --i) v = (v << 32) | w[i];
  const __int128 one = 1;
  if (v & (one << (bits - 1))) v -= (one << bits);
  return v;
}

/** The numerator the oracle would compute at the centre of pixel (px, py). */
__int128 oracle_num(const Tri& t, int64_t px, int64_t py) {
  const int64_t sx = centre(px), sy = centre(py);
  const int64_t w0 = orient(t.bx, t.by, t.cx, t.cy, sx, sy);
  const int64_t w1 = orient(t.cx, t.cy, t.ax, t.ay, sx, sy);
  const int64_t w2 = orient(t.ax, t.ay, t.bx, t.by, sx, sy);
  return static_cast<__int128>(w0) * t.va + static_cast<__int128>(w1) * t.vb +
         static_cast<__int128>(w2) * t.vc;
}

/** The plane GEOM.ATTRSETUP emits: anchored at the ORIGIN, gradients per pixel. */
struct Plane {
  __int128 n0, dndx, dndy;
};

Plane setup_plane(const Tri& t) {
  auto num_at_coord = [&](int64_t sx, int64_t sy) -> __int128 {
    const int64_t w0 = orient(t.bx, t.by, t.cx, t.cy, sx, sy);
    const int64_t w1 = orient(t.cx, t.cy, t.ax, t.ay, sx, sy);
    const int64_t w2 = orient(t.ax, t.ay, t.bx, t.by, sx, sy);
    return static_cast<__int128>(w0) * t.va + static_cast<__int128>(w1) * t.vb +
           static_cast<__int128>(w2) * t.vc;
  };
  Plane p;
  p.n0 = num_at_coord(0, 0);
  // One PIXEL of step is 256 coordinate units, which is exactly the shift
  // ATTRSETUP applies as its last act.
  p.dndx = num_at_coord(256, 0) - p.n0;
  p.dndy = num_at_coord(0, 256) - p.n0;
  return p;
}

/** The oracle's fill rule, so section 1's coverage is a real triangle's. */
bool covered(const Tri& t, int64_t px, int64_t py) {
  const int64_t sx = centre(px), sy = centre(py);
  const int64_t w0 = orient(t.bx, t.by, t.cx, t.cy, sx, sy);
  const int64_t w1 = orient(t.cx, t.cy, t.ax, t.ay, sx, sy);
  const int64_t w2 = orient(t.ax, t.ay, t.bx, t.by, sx, sy);
  auto top_left = [](int64_t ux, int64_t uy, int64_t vx, int64_t vy) {
    const int64_t dx = vx - ux, dy = vy - uy;
    return (dy > 0) || (dy == 0 && dx < 0);
  };
  const int64_t b0 = top_left(t.bx, t.by, t.cx, t.cy) ? 0 : -1;
  const int64_t b1 = top_left(t.cx, t.cy, t.ax, t.ay) ? 0 : -1;
  const int64_t b2 = top_left(t.ax, t.ay, t.bx, t.by) ? 0 : -1;
  return (w0 + b0 >= 0) && (w1 + b1 >= 0) && (w2 + b2 >= 0);
}

void reset(Vzhao_raster_attrinterp& t) {
  t.rst_n = 0;
  t.job_valid_i = 0;
  t.cov_valid_i = 0;
  t.n_ready_i = 0;
  t.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();
}

/**
 * Push one job and its coverage rows through, collecting every emitted pixel.
 * `ready_pattern` cycles over n_ready_i; ~0 means never stall.
 */
bool run_job(Vzhao_raster_attrinterp& t, const Plane& p, int tile_x, int tile_y,
             const std::vector<std::pair<int, uint16_t>>& rows, std::vector<Emit>* out,
             uint32_t ready_pattern) {
  reset(t);
  put_wide(t.job_n0_i.data(), 3, p.n0);
  put_wide(t.job_dndx_i.data(), 3, p.dndx);
  put_wide(t.job_dndy_i.data(), 3, p.dndy);
  t.job_tile_x_i = static_cast<uint16_t>(tile_x) & 0xFFFu;
  t.job_tile_y_i = static_cast<uint16_t>(tile_y) & 0xFFFu;
  t.job_valid_i = 1;
  for (int i = 0; i < 20; ++i) {
    t.eval();
    if (t.job_ready_o) {
      zhao::tick(t);
      break;
    }
    zhao::tick(t);
  }
  t.job_valid_i = 0;

  size_t next = 0;
  int64_t clocks = 0;
  int pat = 0;
  while (clocks < 4000) {
    const bool offering = next < rows.size();
    if (offering) {
      t.cov_row_i = static_cast<uint8_t>(rows[next].first);
      t.cov_mask_i = rows[next].second;
      t.cov_last_i = (next + 1 == rows.size()) ? 1 : 0;
    }
    t.cov_valid_i = offering ? 1 : 0;
    t.n_ready_i = ((ready_pattern >> (pat & 31)) & 1u) ? 1 : 0;
    t.eval();
    const bool took = offering && t.cov_ready_o;
    if (t.n_valid_o && t.n_ready_i)
      out->push_back({t.n_row_o, t.n_col_o, get_wide(t.n_num_o.data(), 96), t.n_last_o != 0});
    const bool done = t.n_valid_o && t.n_ready_i && t.n_last_o;
    zhao::tick(t);
    ++clocks;
    ++pat;
    if (took) ++next;
    if (done) {
      t.cov_valid_i = 0;
      t.eval();
      return true;
    }
  }
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_raster_attrinterp top;

  auto px = [](double p) { return static_cast<int64_t>(p * 256.0); };

  // ------------------------------------------------------------------ 1 ---
  printf("== section 1: the plane lands on the oracle's own sample points ==\n");
  long total_pixels = 0;
  long nonzero_gradient_tris = 0;
  {
    // Tiles deliberately away from the origin: at tile (0,0) a missing
    // half-pixel term is the ONLY error left, and at a distant tile a wrong
    // tile-placement multiply shows up as well.
    const Tri tris[] = {
        {px(0), px(0), px(64), px(0), px(0), px(48), 0, 1 << 24, -(1 << 24), "right triangle"},
        {px(3), px(5), px(90), px(7), px(11), px(60), 12345, -98765, 4242424, "oblique"},
        {px(-8), px(-4), px(70), px(1), px(2), px(50), 1 << 28, -(1 << 28), 7, "off-canvas apex"},
        {px(16), px(16), px(300), px(20), px(24), px(280), -(1 << 26), 1 << 26, 99, "large, steep"},
    };
    const int tiles[][2] = {{0, 0}, {16, 16}, {48, 32}, {256, 128}};

    long bad = 0, cases = 0;
    for (const Tri& t : tris) {
      const Plane p = setup_plane(t);
      if (p.dndx != 0 || p.dndy != 0) ++nonzero_gradient_tris;
      for (const auto& tl : tiles) {
        // Real coverage, from the oracle's own fill rule, so this is a
        // composition test and not just an arithmetic one.
        std::vector<std::pair<int, uint16_t>> rows;
        for (int r = 0; r < 16; ++r) {
          uint16_t m = 0;
          for (int c = 0; c < 16; ++c)
            if (covered(t, tl[0] + c, tl[1] + r)) m |= static_cast<uint16_t>(1u << c);
          if (m) rows.push_back({r, m});
        }
        if (rows.empty()) continue;

        std::vector<Emit> got;
        if (!run_job(top, p, tl[0], tl[1], rows, &got, ~0u)) {
          printf("      %s at tile (%d,%d): TIMED OUT\n", t.what, tl[0], tl[1]);
          ++bad;
          continue;
        }
        for (const Emit& e : got) {
          const __int128 want = oracle_num(t, tl[0] + e.col, tl[1] + e.row);
          if (e.num != want) {
            if (bad < 4)
              printf("      %s tile(%d,%d) px(%d,%d): plane and oracle differ\n", t.what, tl[0],
                     tl[1], e.col, e.row);
            ++bad;
          }
          ++total_pixels;
        }
        ++cases;
      }
    }
    zhao::check(bad == 0,
                "every stepped numerator equals the oracle's at the PIXEL CENTRE", 0,
                (uint32_t)bad);
    printf("   MEASURED: %ld pixels over %ld tile-jobs\n", total_pixels, cases);
  }

  // ------------------------------------------------------------------ 2 ---
  printf("== section 2: the emitted stream IS the coverage mask, in raster order ==\n");
  {
    // Masks chosen for the ways a column walk breaks: both end bits, a single
    // interior bit, alternating bits, and a full row.
    const std::vector<std::pair<int, uint16_t>> rows = {
        {0, 0x0001}, {2, 0x8000}, {5, 0x8001}, {7, 0x0100},
        {9, 0xAAAA}, {11, 0x5555}, {13, 0xFFFF}, {15, 0x0FF0},
    };
    const Tri t = {px(3), px(5), px(90), px(7), px(11), px(60), 12345, -98765, 4242424, "stream"};
    const Plane p = setup_plane(t);

    std::vector<Emit> got;
    const bool ok = run_job(top, p, 32, 48, rows, &got, ~0u);
    zhao::check(ok, "the job completes", 1, ok ? 1 : 0);

    // Build the expected (row, col) sequence straight from the masks.
    std::vector<std::pair<int, int>> want;
    for (const auto& r : rows)
      for (int c = 0; c < 16; ++c)
        if (r.second & (1u << c)) want.push_back({r.first, c});

    long mismatched = 0;
    for (size_t i = 0; i < got.size() && i < want.size(); ++i)
      if (got[i].row != want[i].first || got[i].col != want[i].second) ++mismatched;
    zhao::check(got.size() == want.size(),
                "exactly one pixel per set mask bit, none dropped or duplicated",
                (uint32_t)want.size(), (uint32_t)got.size());
    zhao::check(mismatched == 0, "and they arrive row by row, column by column", 0,
                (uint32_t)mismatched);

    // n_last_o on the FINAL COVERED pixel, which here is column 11 of row 15 --
    // not column 15, which is not covered by 0x0FF0.
    long lasts = 0;
    for (size_t i = 0; i < got.size(); ++i)
      if (got[i].last) {
        ++lasts;
        if (i + 1 != got.size()) printf("      last fired early, at index %zu\n", i);
      }
    zhao::check(lasts == 1, "n_last_o fires exactly once", 1, (uint32_t)lasts);
    zhao::check(!got.empty() && got.back().last && got.back().col == 11 && got.back().row == 15,
                "and it fires on the last COVERED pixel, not the last column", 1,
                (!got.empty() && got.back().last && got.back().col == 11) ? 1 : 0);
  }

  // ------------------------------------------------------------------ 3 ---
  printf("== section 3: a stalling consumer changes nothing but the clock ==\n");
  {
    const Tri t = {px(3), px(5), px(90), px(7), px(11), px(60), 12345, -98765, 4242424, "stall"};
    const Plane p = setup_plane(t);
    const std::vector<std::pair<int, uint16_t>> rows = {
        {1, 0x0F0F}, {4, 0xFFFF}, {6, 0x8001}, {12, 0x0240}};

    std::vector<Emit> fast, slow;
    const bool a = run_job(top, p, 64, 16, rows, &fast, ~0u);
    const bool b = run_job(top, p, 64, 16, rows, &slow, 0x8C1A5303u);
    zhao::check(a && b, "both runs complete", 1, (a && b) ? 1 : 0);

    long diff = 0;
    if (fast.size() != slow.size())
      diff = 1;
    else
      for (size_t i = 0; i < fast.size(); ++i)
        if (fast[i].row != slow[i].row || fast[i].col != slow[i].col ||
            fast[i].num != slow[i].num || fast[i].last != slow[i].last)
          ++diff;
    zhao::check(diff == 0, "backpressure produces a byte-identical stream", 0, (uint32_t)diff);
  }

  // ------------------------------------------------------------------ 4 ---
  printf("== section 4: the gradients were real, so section 1 could have failed ==\n");
  {
    // ANTI-VACUITY. A half-pixel error is invisible on a flat plane, so section
    // 1 only means something if its triangles had gradients big enough for half
    // a step to change the numerator. Check that directly: half a step must be
    // a non-zero part of what the block emits.
    zhao::check(nonzero_gradient_tris >= 3,
                "section 1's triangles had non-zero attribute gradients", 3,
                (uint32_t)nonzero_gradient_tris);
    zhao::check(total_pixels > 200, "and enough covered pixels to exercise the walk", 1,
                total_pixels > 200 ? 1 : 0);

    // The direct statement of the trap: a corner-sampled plane differs from a
    // centre-sampled one by exactly (dNdx + dNdy)/2, and that quantity is not
    // zero for these triangles -- so a block that omitted it WOULD have failed.
    const Tri t = {px(16), px(16), px(300), px(20), px(24), px(280), -(1 << 26), 1 << 26, 99,
                   "steep"};
    const Plane p = setup_plane(t);
    const __int128 half = (p.dndx >> 1) + (p.dndy >> 1);
    zhao::check(half != 0, "the half-pixel term is non-zero, so omitting it is detectable", 1,
                half != 0 ? 1 : 0);
  }

  return zhao::report_and_exit("raster_attrinterp_directed");
}
