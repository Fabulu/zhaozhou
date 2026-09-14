// raster_attrgrad_v2_directed.cpp -- current rast.cpp divider/row differential.
//
// The value oracle is the compiled zref::render::div_rhu_s128 declaration from
// reference/src/zrender/internal.hpp.  Local arithmetic is used only to expose
// saturation and to construct the row numerators handed to that oracle.
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include "Vtb_raster_attrgrad_v2.h"
#include "verilated.h"
#include "zrender/internal.hpp"

using i128 = __int128;
using u128 = unsigned __int128;

double sc_time_stamp() { return 0.0; }

static Vtb_raster_attrgrad_v2* dut = nullptr;  // intentionally heap-resident
static uint64_t cycles = 0;
static int fails = 0;

static void fail(const char* what) {
  std::printf("FAIL: %s\n", what);
  ++fails;
}

static void tick() {
  dut->clk = 0;
  dut->eval();
  dut->clk = 1;
  dut->eval();
  ++cycles;
}

template <size_t N>
static void put_wide(VlWide<N>& dst, i128 value) {
  const u128 bits = static_cast<u128>(value);
  for (size_t i = 0; i < N; ++i) dst[i] = static_cast<uint32_t>(bits >> (32 * i));
}

static int32_t as_i32(uint32_t bits) {
  union {
    uint32_t u;
    int32_t s;
  } cvt{};
  cvt.u = bits;
  return cvt.s;
}

static int32_t wrap32(i128 value) { return as_i32(static_cast<uint32_t>(static_cast<u128>(value))); }

static i128 floor_div(i128 n, i128 d) {
  i128 q = n / d;
  const i128 r = n % d;
  if (r < 0) --q;
  return q;
}

static i128 floor_half(i128 n) {
  if (n >= 0) return n / 2;
  return -(((-n) + 1) / 2);
}

static i128 raw_current_q(i128 n, uint64_t area) {
  return floor_div(n + static_cast<i128>(area / 2), static_cast<i128>(area));
}

static bool current_saturates(i128 n, uint64_t area) {
  const i128 q = raw_current_q(n, area);
  return q > static_cast<i128>(INT32_MAX) || q < static_cast<i128>(INT32_MIN);
}

struct DivResult {
  int32_t q;
  bool saturated;
  bool error;
};

static DivResult run_div(i128 n, uint64_t area) {
  put_wide(dut->d_num_i, n);
  dut->d_area_i = area;
  dut->d_valid_i = 1;
  dut->d_rready_i = 0;

  bool accepted = false;
  for (int guard = 0; guard < 1000; ++guard) {
    dut->eval();
    const bool fire = dut->d_valid_i && dut->d_ready_o;
    tick();
    if (fire) {
      accepted = true;
      break;
    }
  }
  if (!accepted) {
    fail("divider request did not accept finitely");
    return {};
  }
  dut->d_valid_i = 0;

  bool arrived = false;
  DivResult result{};
  for (int guard = 0; guard < 1000; ++guard) {
    dut->eval();
    if (dut->d_rvalid_o) {
      result.q = static_cast<int32_t>(dut->d_q_o);
      result.saturated = dut->d_saturated_o != 0;
      result.error = dut->d_error_o != 0;
      arrived = true;
      break;
    }
    tick();
  }
  if (!arrived) {
    fail("divider response did not arrive finitely");
    return {};
  }

  dut->d_rready_i = 1;
  dut->eval();
  if (!dut->d_rvalid_o) fail("divider response vanished before acceptance");
  tick();
  dut->d_rready_i = 0;
  return result;
}

static void prove_divider_blocks_followup() {
  constexpr i128 first_n = -3;
  constexpr uint64_t first_area = 2;
  constexpr i128 second_n = 29;
  constexpr uint64_t second_area = 7;
  const int32_t first_want =
      zref::render::div_rhu_s128(first_n, static_cast<i128>(first_area));
  const int32_t second_want =
      zref::render::div_rhu_s128(second_n, static_cast<i128>(second_area));

  const uint32_t divides_before = dut->d_divides_o;
  const uint32_t saturations_before = dut->d_saturations_o;
  const uint32_t errors_before = dut->d_errors_o;

  put_wide(dut->d_num_i, first_n);
  dut->d_area_i = first_area;
  dut->d_valid_i = 1;
  dut->d_rready_i = 0;
  bool first_accepted = false;
  for (int guard = 0; guard < 1000; ++guard) {
    dut->eval();
    const bool fire = dut->d_valid_i && dut->d_ready_o;
    tick();
    if (fire) {
      first_accepted = true;
      break;
    }
  }
  if (!first_accepted) {
    fail("held-response first request did not accept finitely");
    dut->d_valid_i = 0;
    return;
  }
  dut->d_valid_i = 0;

  bool first_arrived = false;
  for (int guard = 0; guard < 1000; ++guard) {
    dut->eval();
    if (dut->d_rvalid_o) {
      first_arrived = true;
      break;
    }
    tick();
  }
  if (!first_arrived) {
    fail("held-response first result did not arrive finitely");
    return;
  }
  if (static_cast<int32_t>(dut->d_q_o) != first_want || dut->d_saturated_o ||
      dut->d_error_o)
    fail("held-response first result was wrong");

  const uint32_t held_divides = dut->d_divides_o;
  const uint32_t held_saturations = dut->d_saturations_o;
  const uint32_t held_errors = dut->d_errors_o;
  const uint32_t held_busy = dut->d_busy_clocks_o;
  if (held_divides != divides_before + 1 || held_saturations != saturations_before ||
      held_errors != errors_before)
    fail("held-response first result counters were not exact");

  // Keep a genuinely distinct successor valid throughout the first result's
  // stall.  It may neither advertise ready nor alter any response/evidence.
  put_wide(dut->d_num_i, second_n);
  dut->d_area_i = second_area;
  dut->d_valid_i = 1;
  dut->d_rready_i = 0;
  for (int held_cycle = 0; held_cycle < 4; ++held_cycle) {
    dut->eval();
    if (!dut->d_rvalid_o || static_cast<int32_t>(dut->d_q_o) != first_want ||
        dut->d_saturated_o || dut->d_error_o)
      fail("divider first result changed while successor was offered");
    if (dut->d_ready_o)
      fail("divider admitted a successor behind a held result");
    if (dut->d_divides_o != held_divides ||
        dut->d_saturations_o != held_saturations ||
        dut->d_errors_o != held_errors || dut->d_busy_clocks_o != held_busy)
      fail("divider evidence changed while successor was blocked");
    tick();
  }

  // Consume only the first result; the successor remains valid.  Ready was low
  // on this edge, so the second request cannot have been admitted simultaneously.
  dut->d_rready_i = 1;
  dut->eval();
  if (!dut->d_rvalid_o || dut->d_ready_o ||
      static_cast<int32_t>(dut->d_q_o) != first_want || dut->d_saturated_o ||
      dut->d_error_o || dut->d_divides_o != held_divides ||
      dut->d_saturations_o != held_saturations ||
      dut->d_errors_o != held_errors || dut->d_busy_clocks_o != held_busy)
    fail("divider first-consume edge violated held-result exclusion");
  tick();
  dut->d_rready_i = 0;

  bool second_accepted = false;
  for (int guard = 0; guard < 1000; ++guard) {
    dut->eval();
    if (dut->d_divides_o != held_divides ||
        dut->d_saturations_o != held_saturations ||
        dut->d_errors_o != held_errors)
      fail("divider counters changed before successor admission");
    const bool fire = dut->d_valid_i && dut->d_ready_o;
    tick();
    if (fire) {
      second_accepted = true;
      break;
    }
  }
  dut->d_valid_i = 0;
  if (!second_accepted) {
    fail("blocked successor did not accept after first result retired");
    return;
  }

  bool second_arrived = false;
  for (int guard = 0; guard < 1000; ++guard) {
    dut->eval();
    if (dut->d_rvalid_o) {
      second_arrived = true;
      break;
    }
    tick();
  }
  if (!second_arrived) {
    fail("accepted successor did not return finitely");
    return;
  }
  if (static_cast<int32_t>(dut->d_q_o) != second_want || dut->d_saturated_o ||
      dut->d_error_o)
    fail("accepted successor result was not exact");
  if (dut->d_divides_o != held_divides + 1 ||
      dut->d_saturations_o != held_saturations ||
      dut->d_errors_o != held_errors)
    fail("accepted successor counters were not exact");

  dut->d_rready_i = 1;
  tick();
  dut->d_rready_i = 0;
}

struct Row {
  uint8_t row;
  uint16_t mask;
};

struct Job {
  i128 n0;
  i128 dx;
  i128 dy;
  uint64_t area;
  int min_x;
  int tile_x;
  int tile_y;
  std::vector<Row> rows;
};

struct Pixel {
  int row;
  int col;
  int32_t q;
  bool saturated;
  bool error;
  bool last;
};

struct ExpectedJob {
  std::vector<Pixel> pixels;
  uint32_t divides = 0;
  uint32_t saturations = 0;
  uint32_t errors = 0;
  int32_t grad = 0;
};

static ExpectedJob expected_job(const Job& j) {
  ExpectedJob e;
  e.divides = 1 + static_cast<uint32_t>(j.rows.size());
  if (j.area == 0) {
    e.errors = e.divides;
    e.grad = 0;
  } else {
    e.grad = zref::render::div_rhu_s128(j.dx, static_cast<i128>(j.area));
    if (current_saturates(j.dx, j.area)) ++e.saturations;
  }

  for (size_t ri = 0; ri < j.rows.size(); ++ri) {
    const Row& row = j.rows[ri];
    const i128 row_n = j.n0 + j.dx * j.min_x +
                       j.dy * (j.tile_y + static_cast<int>(row.row)) +
                       floor_half(j.dx) + floor_half(j.dy);
    int32_t row_q = 0;
    bool row_sat = false;
    if (j.area != 0) {
      row_q = zref::render::div_rhu_s128(row_n, static_cast<i128>(j.area));
      row_sat = current_saturates(row_n, j.area);
      if (row_sat) ++e.saturations;
    }
    int32_t q = wrap32(static_cast<i128>(row_q) +
                       static_cast<i128>(e.grad) * (j.tile_x - j.min_x));
    int last_col = -1;
    for (int c = 0; c < 16; ++c)
      if ((row.mask >> c) & 1u) last_col = c;
    for (int c = 0; c < 16; ++c) {
      if ((row.mask >> c) & 1u) {
        e.pixels.push_back(Pixel{static_cast<int>(row.row), c, q,
                                 (j.area != 0) &&
                                     (current_saturates(j.dx, j.area) || row_sat),
                                 j.area == 0,
                                 ri + 1 == j.rows.size() && c == last_col});
      }
      q = wrap32(static_cast<i128>(q) + e.grad);
    }
  }
  return e;
}

struct RunJobResult {
  std::vector<Pixel> pixels;
  uint32_t divides;
  uint32_t saturations;
  uint32_t errors;
  uint32_t pixel_count;
  int stall_cycles;
};

static RunJobResult run_job(const Job& j, uint64_t stall_seed) {
  const uint32_t d0 = dut->g_divides_o;
  const uint32_t s0 = dut->g_saturations_o;
  const uint32_t e0 = dut->g_divide_errors_o;
  const uint32_t p0 = dut->g_pixels_o;

  put_wide(dut->g_job_n0_i, j.n0);
  put_wide(dut->g_job_dndx_i, j.dx);
  put_wide(dut->g_job_dndy_i, j.dy);
  dut->g_job_area2_i = j.area;
  dut->g_job_min_x_i = static_cast<uint16_t>(j.min_x) & 0x0fff;
  dut->g_job_tile_x_i = static_cast<uint16_t>(j.tile_x) & 0x0fff;
  dut->g_job_tile_y_i = static_cast<uint16_t>(j.tile_y) & 0x0fff;
  dut->g_job_valid_i = 1;

  bool job_accepted = false;
  for (int guard = 0; guard < 1000; ++guard) {
    dut->eval();
    const bool fire = dut->g_job_valid_i && dut->g_job_ready_o;
    tick();
    if (fire) {
      job_accepted = true;
      break;
    }
  }
  if (!job_accepted) fail("gradient job did not accept finitely");
  dut->g_job_valid_i = 0;

  std::mt19937_64 rng(stall_seed);
  size_t next_row = 0;
  bool row_offered = false;
  bool hold_active = false;
  Pixel held{};
  bool forced_stall = false;
  int stall_cycles = 0;
  std::vector<Pixel> got;

  for (int guard = 0; guard < 200000; ++guard) {
    if (!row_offered && next_row < j.rows.size()) {
      dut->g_cov_valid_i = 1;
      dut->g_cov_row_i = j.rows[next_row].row;
      dut->g_cov_mask_i = j.rows[next_row].mask;
      dut->g_cov_last_i = (next_row + 1 == j.rows.size());
      row_offered = true;
    }

    dut->g_q_ready_i = (rng() & 3u) != 0;
    dut->eval();
    if (dut->g_q_valid_o && !forced_stall) {
      dut->g_q_ready_i = 0;
      dut->eval();
      forced_stall = true;
    }

    const Pixel now{static_cast<int>(dut->g_q_row_o),
                    static_cast<int>(dut->g_q_col_o),
                    static_cast<int32_t>(dut->g_q_o),
                    dut->g_q_saturated_o != 0,
                    dut->g_q_error_o != 0,
                    dut->g_q_last_o != 0};
    if (hold_active) {
      if (!dut->g_q_valid_o || now.row != held.row || now.col != held.col ||
          now.q != held.q || now.saturated != held.saturated ||
          now.error != held.error || now.last != held.last)
        fail("gradient output changed under backpressure");
    }

    const bool cov_fire = dut->g_cov_valid_i && dut->g_cov_ready_o;
    const bool q_fire = dut->g_q_valid_o && dut->g_q_ready_i;
    if (dut->g_q_valid_o && !dut->g_q_ready_i) {
      held = now;
      hold_active = true;
      ++stall_cycles;
    } else {
      hold_active = false;
    }

    tick();
    if (cov_fire) {
      ++next_row;
      row_offered = false;
      dut->g_cov_valid_i = 0;
    }
    if (q_fire) got.push_back(now);

    dut->eval();
    if (next_row == j.rows.size() && !row_offered && dut->g_idle_o &&
        !dut->g_q_valid_o) {
      dut->g_q_ready_i = 0;
      dut->g_cov_valid_i = 0;
      return RunJobResult{got,
                          static_cast<uint32_t>(dut->g_divides_o - d0),
                          static_cast<uint32_t>(dut->g_saturations_o - s0),
                          static_cast<uint32_t>(dut->g_divide_errors_o - e0),
                          static_cast<uint32_t>(dut->g_pixels_o - p0),
                          stall_cycles};
    }
  }

  fail("gradient job did not drain finitely");
  return RunJobResult{got, 0, 0, 0, 0, stall_cycles};
}

static int compare_job(const ExpectedJob& want, const RunJobResult& got, bool report) {
  int mismatches = 0;
  if (want.pixels.size() != got.pixels.size()) ++mismatches;
  const size_t count = want.pixels.size() < got.pixels.size()
                           ? want.pixels.size()
                           : got.pixels.size();
  for (size_t i = 0; i < count; ++i) {
    const Pixel& w = want.pixels[i];
    const Pixel& g = got.pixels[i];
    if (w.row != g.row || w.col != g.col || w.q != g.q ||
        w.saturated != g.saturated || w.error != g.error || w.last != g.last) {
      ++mismatches;
      if (report && mismatches <= 5)
        std::printf("  pixel mismatch %zu: got (%d,%d q=%ld sat=%d err=%d last=%d), "
                    "want (%d,%d q=%ld sat=%d err=%d last=%d)\n",
                    i, g.row, g.col, static_cast<long>(g.q), g.saturated, g.error,
                    g.last, w.row, w.col, static_cast<long>(w.q), w.saturated,
                    w.error, w.last);
    }
  }
  if (want.divides != got.divides || want.saturations != got.saturations ||
      want.errors != got.errors || want.pixels.size() != got.pixel_count) {
    ++mismatches;
    if (report)
      std::printf("  counter mismatch: div %u/%u sat %u/%u err %u/%u pix %u/%zu\n",
                  got.divides, want.divides, got.saturations, want.saturations,
                  got.errors, want.errors, got.pixel_count, want.pixels.size());
  }
  if (got.stall_cycles == 0) {
    ++mismatches;
    if (report) std::printf("  no output stall was exercised\n");
  }
  return mismatches;
}

static Job cross_tile_job() {
  // At min_x=0: row numerator is zero and grad=round(6/10)=1.  Two exact s32
  // steps give tile-col0=2, while a forbidden fresh divide gives round(12/10)=1.
  return Job{-3, 6, 0, 10, 0, 2, 0, {{0, 0x8001}}};
}

static void hard_exit(int rc) {
  std::fflush(stdout);
  // MinGW/Verilator teardown has hung on this machine.  The model is deliberately
  // heap-resident and the process exits after all checks and output are flushed.
  std::_Exit(rc);
}

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  dut = new Vtb_raster_attrgrad_v2;
  dut->clk = 0;
  dut->rst_n = 0;
  dut->d_valid_i = 0;
  dut->d_rready_i = 0;
  dut->g_job_valid_i = 0;
  dut->g_cov_valid_i = 0;
  dut->g_q_ready_i = 0;
  for (int i = 0; i < 5; ++i) tick();
  dut->rst_n = 1;
  tick();

#ifdef EXPECT_NEG_HALF_MUTANT
  const DivResult got = run_div(-3, 2);
  const int32_t oracle = zref::render::div_rhu_s128(-3, 2);
  if (oracle != -1 || got.q != -2 || got.error || got.saturated || fails != 0) {
    std::printf("NEG-HALF MUTANT DID NOT FIRE: rtl=%ld oracle=%ld\n",
                static_cast<long>(got.q), static_cast<long>(oracle));
    hard_exit(1);
  }
  std::printf("PASS: negative exact-half mutant fired (rtl=-2, current zref=-1)\n");
  hard_exit(0);
#endif

#ifdef EXPECT_OMIT_MIN_X_ACCUM_MUTANT
  const Job j = cross_tile_job();
  const ExpectedJob want = expected_job(j);
  const RunJobResult got = run_job(j, 0x7711);
  const i128 row_n = j.n0 + floor_half(j.dx);
  const int32_t forbidden_fresh = zref::render::div_rhu_s128(
      row_n + j.dx * (j.tile_x - j.min_x), static_cast<i128>(j.area));
  const int exact_mismatches = compare_job(want, got, false);

  const bool want_signature =
      want.pixels.size() == 2 && want.pixels[0].row == 0 &&
      want.pixels[0].col == 0 && want.pixels[0].q == 2 &&
      !want.pixels[0].saturated && !want.pixels[0].error &&
      !want.pixels[0].last && want.pixels[1].row == 0 &&
      want.pixels[1].col == 15 && want.pixels[1].q == 17 &&
      !want.pixels[1].saturated && !want.pixels[1].error &&
      want.pixels[1].last && want.divides == 2 && want.saturations == 0 &&
      want.errors == 0;
  const bool mutant_signature =
      got.pixels.size() == 2 && got.pixels[0].row == 0 &&
      got.pixels[0].col == 0 && got.pixels[0].q == 0 &&
      !got.pixels[0].saturated && !got.pixels[0].error &&
      !got.pixels[0].last && got.pixels[1].row == 0 &&
      got.pixels[1].col == 15 && got.pixels[1].q == 15 &&
      !got.pixels[1].saturated && !got.pixels[1].error &&
      got.pixels[1].last && got.divides == 2 && got.saturations == 0 &&
      got.errors == 0 && got.pixel_count == 2 && got.stall_cycles > 0;

  if (!want_signature || !mutant_signature || forbidden_fresh != 1 ||
      exact_mismatches != 2 || fails != 0) {
    std::printf("OMIT-MIN-X-ACCUM MUTANT DID NOT FIRE EXACTLY: "
                "mismatches=%d fails=%d\n",
                exact_mismatches, fails);
    hard_exit(1);
  }
  std::printf("PASS: omitted global-min-X tile offset mutant fired exactly "
              "(q={0,15}, stepped={2,17}, fresh=1, mismatches=2)\n");
  hard_exit(0);
#endif

  std::printf("== V2 divider against actual zref::render::div_rhu_s128 ==\n");
  prove_divider_blocks_followup();
  uint32_t expected_divides = dut->d_divides_o;
  uint32_t expected_saturations = dut->d_saturations_o;
  uint32_t expected_errors = dut->d_errors_o;
  int positive_cases = 0, negative_cases = 0, negative_half_cases = 0;

  auto check_div = [&](i128 n, uint64_t area) {
    const DivResult got = run_div(n, area);
    ++expected_divides;
    if (area == 0) {
      ++expected_errors;
      if (!got.error || got.saturated || got.q != 0) fail("zero-area terminal result wrong");
    } else {
      const int32_t want = zref::render::div_rhu_s128(n, static_cast<i128>(area));
      const bool sat = current_saturates(n, area);
      if (sat) ++expected_saturations;
      if (n < 0) ++negative_cases;
      else ++positive_cases;
      if (got.q != want || got.saturated != sat || got.error)
        fail("divider disagreed with actual zref");
    }
    if (dut->d_divides_o != expected_divides ||
        dut->d_saturations_o != expected_saturations ||
        dut->d_errors_o != expected_errors)
      fail("divider evidence counters were not exact");
  };

  check_div(0, 1);
  check_div(1, 2);
  check_div(-1, 2);
  check_div(3, 2);
  check_div(-3, 2); ++negative_half_cases;
  check_div(-5, 2); ++negative_half_cases;
  check_div(7, 4);
  check_div(-6, 4);

  const i128 rail_area = 12346;
  check_div(static_cast<i128>(INT32_MAX) * rail_area - rail_area / 2, rail_area);
  check_div((static_cast<i128>(INT32_MAX) + 1) * rail_area - rail_area / 2, rail_area);
  check_div(static_cast<i128>(INT32_MIN) * rail_area - rail_area / 2, rail_area);
  check_div((static_cast<i128>(INT32_MIN) - 1) * rail_area - rail_area / 2, rail_area);
  check_div(77, 0);

  std::mt19937_64 rng(0xD1A7'2026'0914ULL);
  for (int i = 0; i < 240; ++i) {
    const uint64_t area = 1 + (rng() & ((uint64_t{1} << 47) - 1));
    i128 n;
    if ((i & 3) != 0) {
      const int64_t q = static_cast<int64_t>(rng() % 4000001) - 2000000;
      const i128 rem = static_cast<i128>(rng() % area);
      n = static_cast<i128>(q) * area + rem;
    } else {
      u128 bits = static_cast<u128>(rng()) | (static_cast<u128>(rng()) << 64);
      bits &= (static_cast<u128>(1) << 96) - 1;
      if ((bits >> 95) & 1) bits |= ~((static_cast<u128>(1) << 96) - 1);
      n = static_cast<i128>(bits);
    }
    check_div(n, area);
  }

  if (positive_cases == 0 || negative_cases == 0 || negative_half_cases < 2)
    fail("divider sign/tie coverage was vacuous");
  if (dut->d_saturations_o < 2 || dut->d_errors_o == 0 || dut->d_busy_clocks_o == 0)
    fail("divider detector positive controls did not fire");

  std::printf("   %u results, %u saturations, %u terminal errors\n",
              dut->d_divides_o, dut->d_saturations_o, dut->d_errors_o);

  std::printf("== V2 rows against current scanline law ==\n");
  std::vector<Job> jobs;
  jobs.push_back(Job{-3, 6, 0, 10, 0, 2, 0, {{0, 0x8001}}});
  jobs.push_back(Job{4500, 901, -233, 997, -3, 5, -4,
                     {{0, 0x8421}, {5, 0x1088}, {15, 0x9001}}});
  jobs.push_back(Job{-8100, -733, 417, 1024, -7, 8, 3,
                     {{1, 0x00f3}, {7, 0x8101}, {13, 0x2222}}});
  // Saturation positive controls for both the x-gradient and row seed.
  jobs.push_back(Job{0, static_cast<i128>(1) << 40, 0, 1, 0, 1, 0,
                     {{2, 0x0005}}});
  // Every requested divide, including each row divide, must terminate in error.
  jobs.push_back(Job{99, -17, 31, 0, 0, 4, -2,
                     {{0, 0x0003}, {9, 0x8000}}});

  for (int i = 0; i < 12; ++i) {
    const uint64_t area = 17 + (rng() % 20000);
    const i128 dx = static_cast<int64_t>(rng() % 20001) - 10000;
    const i128 dy = static_cast<int64_t>(rng() % 20001) - 10000;
    const i128 n0 = static_cast<int64_t>(rng() % 2000001) - 1000000;
    const int min_x = static_cast<int>(rng() % 33) - 16;
    const int tile_x = min_x + static_cast<int>(rng() % 25) - 4;
    const int tile_y = static_cast<int>(rng() % 33) - 16;
    std::vector<Row> rows;
    for (int r = 0; r < 4; ++r) {
      uint16_t mask = static_cast<uint16_t>(rng());
      if (mask == 0) mask = 1;
      rows.push_back(Row{static_cast<uint8_t>((r * 5 + i) & 15), mask});
    }
    jobs.push_back(Job{n0, dx, dy, area, min_x, tile_x, tile_y, rows});
  }

  uint64_t checked_pixels = 0;
  int row_mismatches = 0;
  for (size_t i = 0; i < jobs.size(); ++i) {
    const ExpectedJob want = expected_job(jobs[i]);
    const RunJobResult got = run_job(jobs[i], 0x9000 + i);
    checked_pixels += want.pixels.size();
    row_mismatches += compare_job(want, got, true);
  }
  if (row_mismatches != 0) fail("one or more gradient jobs mismatched");

  const Job cross = cross_tile_job();
  const i128 cross_row_n = cross.n0 + floor_half(cross.dx);
  const int32_t fresh = zref::render::div_rhu_s128(
      cross_row_n + cross.dx * (cross.tile_x - cross.min_x),
      static_cast<i128>(cross.area));
  const int32_t stepped = expected_job(cross).pixels[0].q;
  if (fresh == stepped || fresh != 1 || stepped != 2)
    fail("cross-tile anti-vacuity vector did not distinguish fresh divide");
  if (dut->g_saturations_o == 0 || dut->g_divide_errors_o == 0)
    fail("gradient detector positive controls did not fire");

  std::printf("   %llu covered pixels; exact row restarts, masks, stalls, and counters\n",
              static_cast<unsigned long long>(checked_pixels));
  std::printf("   cross-tile: stepped=%ld, forbidden fresh divide=%ld\n",
              static_cast<long>(stepped), static_cast<long>(fresh));
  std::printf("%s: %llu clocks\n", fails == 0 ? "PASS" : "FAIL",
              static_cast<unsigned long long>(cycles));
  hard_exit(fails == 0 ? 0 : 1);
}
