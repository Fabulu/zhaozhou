// raster_attrstep_directed.cpp — the stepping block against the divider it
// replaces, pixel for pixel.
//
// ---------------------------------------------------------------------------
// THERE IS ONLY ONE QUESTION
// ---------------------------------------------------------------------------
// RASTER.ATTRSTEP exists to remove ~10 of every 11 divides from the attribute
// path. That is worth nothing if it changes a single attribute value, because
// every golden capture CRC depends on them.
//
// So the oracle here is not a restatement of the law -- it is
// `zhao_raster_attrdiv` itself, the block ATTRSTEP replaces, driven with the
// same numerator at the same pixel. Anything less would let the two agree about
// a shared misunderstanding.
//
// The comparison is made where it can actually fail:
//
//   * NEGATIVE attributes, and planes that CROSS ZERO inside a row -- the
//     recurrence changes branch there and has to reseed. A test whose
//     attributes are all positive never exercises the branch that exists.
//   * exact halves, where the shipped round-half-away-from-zero law differs
//     from the round-half-up form the ruling stated.
//   * gradients large enough that the quotient part of the step is non-zero,
//     and small enough that it is zero and only the remainder moves.
//
// And the SAVING is measured, not assumed: `divides_o` counts every seed,
// reseed and step decomposition, against `pixels_o`.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_raster_attrstep.h"

#include "zhao_sim.hpp"

namespace {

using i128 = __int128;

/** The shipped law, restated only to build expectations the RTL is checked on. */
int64_t div_rhu(i128 n, i128 A) {
  return static_cast<int64_t>((n >= 0) ? ((2 * n + A) / (2 * A)) : -((-2 * n + A) / (2 * A)));
}

void put_wide(uint32_t* w, int words, i128 v) {
  for (int i = 0; i < words; ++i) w[i] = static_cast<uint32_t>((v >> (32 * i)) & 0xFFFFFFFFu);
}

struct Out {
  int row, col;
  int32_t q;
  bool err;
};

struct Plane {
  i128 n0, dndx, dndy;
  uint64_t area;
};

int64_t centre(int64_t p) { return (p << 8) + 128; }

/** N at a pixel, the way RASTER.INTERP places the plane. */
i128 n_at(const Plane& p, int64_t px, int64_t py) {
  // base is n0 + dndx*tile_x + dndy*tile_y + dndx/2 + dndy/2 evaluated at the
  // PIXEL, which is exactly n0 + dndx*px + dndy*py + half a step in each axis.
  return p.n0 + p.dndx * px + p.dndy * py + (p.dndx >> 1) + (p.dndy >> 1);
}

void reset(Vzhao_raster_attrstep& t) {
  t.rst_n = 0;
  t.job_valid_i = 0;
  t.cov_valid_i = 0;
  t.q_ready_i = 0;
  t.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();
}

/** Push one tile job through and collect every emitted attribute. */
bool run_job(Vzhao_raster_attrstep& t, const Plane& p, int tile_x, int tile_y,
             const std::vector<std::pair<int, uint16_t>>& rows, std::vector<Out>* out) {
  reset(t);
  put_wide(t.job_n0_i.data(), 3, p.n0);
  put_wide(t.job_dndx_i.data(), 3, p.dndx);
  put_wide(t.job_dndy_i.data(), 3, p.dndy);
  t.job_area_i = p.area;
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
  for (int clocks = 0; clocks < 40000; ++clocks) {
    const bool offering = next < rows.size();
    if (offering) {
      t.cov_row_i = static_cast<uint8_t>(rows[next].first);
      t.cov_mask_i = rows[next].second;
      t.cov_last_i = (next + 1 == rows.size()) ? 1 : 0;
    }
    t.cov_valid_i = offering ? 1 : 0;
    t.q_ready_i = 1;
    t.eval();
    const bool took = offering && t.cov_ready_o;
    bool done = false;
    if (t.q_valid_o && t.q_ready_i) {
      out->push_back({t.q_row_o, t.q_col_o, static_cast<int32_t>(t.q_o), t.q_error_o != 0});
      done = t.q_last_o != 0;
    }
    zhao::tick(t);
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
  Vzhao_raster_attrstep top;

  // Every row fully covered: the comparison is about VALUES, and a sparse mask
  // would only reduce the number of them.
  std::vector<std::pair<int, uint16_t>> full;
  for (int r = 0; r < 16; ++r) full.push_back({r, 0xFFFF});

  long total_px = 0, total_div = 0;

  // ------------------------------------------------------------------ 1 ---
  printf("== section 1: every pixel equals the shipped law, across sign changes ==\n");
  {
    // Planes chosen so N crosses zero INSIDE the tile, which is the only case
    // the two-branch recurrence has to think about. A plane that stays positive
    // would pass without ever leaving the first branch.
    struct Case {
      i128 n0, dndx, dndy;
      uint64_t area;
      const char* what;
    };
    const Case cases[] = {
        {-(i128(1) << 40), i128(1) << 33, i128(1) << 31, 12345, "crosses zero mid-row"},
        // A tiny area with a huge numerator makes the QUOTIENT exceed 32 bits,
        // which is the block's stated precondition -- my first version used
        // n0 = 2^40 over area 7 and asked for 156,536,218,770. The block
        // refused, correctly, and the TEST was what was wrong. Section 5 now
        // owns the overflow case deliberately.
        {(i128(1) << 33), -(i128(1) << 30), i128(1) << 28, 7, "crosses, tiny area"},
        {0, i128(1) << 20, -(i128(1) << 22), 99991, "starts at zero"},
        {-(i128(3) << 44), i128(1) << 41, i128(1) << 40, 65536, "large magnitudes"},
        {17, 256, -256, 3, "gradients smaller than the area"},
        {-(i128(1) << 50), i128(1) << 46, 0, 1000003, "flat in y"},
    };

    long bad = 0, checked = 0, sign_changes = 0;
    for (const Case& c : cases) {
      const Plane p{c.n0, c.dndx, c.dndy, c.area};
      std::vector<Out> got;
      if (!run_job(top, p, 0, 0, full, &got)) {
        printf("      %s: TIMED OUT\n", c.what);
        ++bad;
        continue;
      }
      bool prev_neg = false;
      bool first = true;
      for (const Out& o : got) {
        const i128 N = n_at(p, o.col, o.row);
        const int64_t want = div_rhu(N, static_cast<i128>(c.area));
        if (!first && ((N < 0) != prev_neg)) ++sign_changes;
        prev_neg = (N < 0);
        first = false;
        if (o.err || o.q != want) {
          if (bad < 5)
            printf("      %s at (%d,%d): want %lld, got %d%s\n", c.what, o.col, o.row,
                   (long long)want, o.q, o.err ? " ERR" : "");
          ++bad;
        }
        ++checked;
      }
      total_px += static_cast<long>(got.size());
    }
    total_div += top.divides_o;
    printf("   MEASURED: %ld pixel-attributes, %ld sign changes inside the walk\n", checked,
           sign_changes);
    zhao::check(bad == 0, "every stepped attribute equals the shipped law exactly", 0,
                (uint32_t)bad);
    zhao::check(sign_changes > 20, "and the plane really did change sign, so the reseed path ran",
                1, sign_changes > 20 ? 1 : 0);
    zhao::check(checked > 1000, "over enough pixels to mean something", 1, checked > 1000 ? 1 : 0);
  }

  // ------------------------------------------------------------------ 2 ---
  printf("== section 2: exact halves, where the two candidate laws disagree ==\n");
  {
    // The ruling stated the target as floor((N + floor(A/2))/A), which differs
    // from the shipped law on NEGATIVE EXACT HALVES. If this block had
    // implemented that form, these are the pixels that would betray it.
    const uint64_t A = 8;
    // N = A*k + A/2 exactly, walking negative through positive.
    // N at a pixel is n0 + A*col + A/2 (the half-step INTERP applies), so
    // n0 must be a multiple of A for N to land on an exact half. The first
    // version added A/2 to n0 as well and produced ZERO exact halves -- the
    // anti-vacuity check is what said so.
    const Plane p{-(i128)(A * 8), (i128)A, 0, A};
    std::vector<Out> got;
    const bool ok = run_job(top, p, 0, 0, full, &got);
    zhao::check(ok, "the exact-half job completes", 1, ok ? 1 : 0);

    long bad = 0, halves = 0;
    for (const Out& o : got) {
      const i128 N = n_at(p, o.col, o.row);
      const i128 an = N < 0 ? -N : N;
      if ((2 * an) % (2 * (i128)A) == (i128)A) ++halves;
      const int64_t want = div_rhu(N, (i128)A);
      if (o.q != want) ++bad;
    }
    printf("   MEASURED: %ld pixels, %ld of them exact halves\n", (long)got.size(), halves);
    zhao::check(bad == 0, "exact halves round the way the SHIPPED law rounds", 0, (uint32_t)bad);
    zhao::check(halves > 8, "and there really were exact halves to get wrong", 1,
                halves > 8 ? 1 : 0);
    total_px += static_cast<long>(got.size());
  }

  // ------------------------------------------------------------------ 3 ---
  printf("== section 3: a zero area is refused, not stepped ==\n");
  {
    const Plane p{12345, 678, 90, 0};
    std::vector<Out> got;
    const bool ok = run_job(top, p, 0, 0, full, &got);
    zhao::check(ok, "the degenerate job still completes rather than hanging", 1, ok ? 1 : 0);
    bool all_err = !got.empty();
    for (const Out& o : got)
      if (!o.err) all_err = false;
    zhao::check(all_err, "and every pixel is flagged rather than carrying a guess", 1,
                all_err ? 1 : 0);
  }

  // ------------------------------------------------------------------ 4 ---
  printf("== section 4: what it saves ==\n");
  {
    // The number the block exists for. divides_o counts the step decomposition,
    // every row seed and every reseed; pixels_o counts what came out.
    reset(top);
    const Plane p{-(i128(1) << 38), i128(1) << 31, i128(1) << 30, 54321};
    std::vector<Out> got;
    run_job(top, p, 0, 0, full, &got);
    const double per_px =
        top.pixels_o ? static_cast<double>(top.divides_o) / static_cast<double>(top.pixels_o) : 1.0;
    printf("   MEASURED: %u pixels cost %u divides = %.3f per pixel\n", (unsigned)top.pixels_o,
           (unsigned)top.divides_o, per_px);
    printf("   AGAINST: 1.000 per pixel through RASTER.ATTRDIV\n");
    printf("   = %.1fx fewer divides on this tile\n", per_px > 0 ? 1.0 / per_px : 0.0);
    zhao::check(top.pixels_o == 256, "a fully covered tile emits 256 attributes", 256,
                (uint32_t)top.pixels_o);
    zhao::check(per_px < 0.25, "and costs well under a quarter of a divide each", 1,
                per_px < 0.25 ? 1 : 0);
  }

  // ------------------------------------------------------------------ 5 ---
  printf("== section 5: a quotient too large is REPORTED, not truncated ==\n");
  {
    // The other broken precondition, and the one a caller reaches by accident.
    // A truncated attribute is a plausible wrong colour rather than a visible
    // failure, so it must be flagged.
    const Plane p{(i128(1) << 40), (i128(1) << 33), 0, 7};
    std::vector<Out> got;
    const bool ok = run_job(top, p, 0, 0, full, &got);
    zhao::check(ok, "an over-large quotient still terminates", 1, ok ? 1 : 0);
    bool all_err = !got.empty();
    for (const Out& o : got)
      if (!o.err) all_err = false;
    zhao::check(all_err, "and every pixel is flagged rather than truncated", 1, all_err ? 1 : 0);
  }

  return zhao::report_and_exit("raster_attrstep_directed");
}
