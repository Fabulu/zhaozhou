// raster_attrgrad_dsp3_diff.cpp -- cycle-exact V2/ATTR3 differential.
#include "Vtb_raster_attrgrad_dsp3_pair.h"
#include "verilated.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

using i128 = __int128;
using u128 = unsigned __int128;

static Vtb_raster_attrgrad_dsp3_pair* dut;
static uint64_t cycles;
static int failures;
static bool held;
static uint32_t held_q;
static uint8_t held_row, held_col;
static bool held_last, held_sat, held_err;

static void hard_exit(int rc);

double sc_time_stamp() { return 0.0; }

template <size_t N>
static void put_wide(VlWide<N>& dst, i128 value) {
  const u128 bits = static_cast<u128>(value);
  for (size_t i = 0; i < N; ++i)
    dst[i] = static_cast<uint32_t>(bits >> (32 * i));
}

static void fail(const char* text) {
  if (failures < 20) {
#ifdef EXPECT_ATTR3_MUTANT
    std::printf("MISMATCH: %s at cycle %llu\n", text,
                static_cast<unsigned long long>(cycles));
#else
    std::printf("FAIL: %s at cycle %llu\n", text,
                static_cast<unsigned long long>(cycles));
#endif
  }
  ++failures;
}

static void compare_low() {
#ifdef EXPECT_ATTR3_OFFSET_GUARD
  if (dut->new_offset_guard_fault_o) {
    std::printf("ATTR3 OFFSET GUARD FIRED at cycle %llu\n",
                static_cast<unsigned long long>(cycles));
    hard_exit(0);
  }
#endif
  if (dut->old_job_ready_o != dut->new_job_ready_o) fail("job ready differs");
  if (dut->old_cov_ready_o != dut->new_cov_ready_o) fail("coverage ready differs");
  if (dut->old_q_valid_o != dut->new_q_valid_o) fail("output valid differs");
  if (dut->old_idle_o != dut->new_idle_o) fail("idle differs");
  if (dut->old_pixels_o != dut->new_pixels_o ||
      dut->old_divides_o != dut->new_divides_o ||
      dut->old_saturations_o != dut->new_saturations_o ||
      dut->old_errors_o != dut->new_errors_o)
    fail("logical counters differ");
  if (dut->old_q_valid_o) {
    if (dut->old_q_o != dut->new_q_o || dut->old_row_o != dut->new_row_o ||
        dut->old_col_o != dut->new_col_o || dut->old_last_o != dut->new_last_o ||
        dut->old_saturated_o != dut->new_saturated_o ||
        dut->old_error_o != dut->new_error_o)
      fail("output payload differs");
    if (held && (dut->old_q_o != held_q || dut->old_row_o != held_row ||
                 dut->old_col_o != held_col || dut->old_last_o != held_last ||
                 dut->old_saturated_o != held_sat || dut->old_error_o != held_err))
      fail("held output changed");
  }
  if (dut->old_q_valid_o && !dut->q_ready_i) {
    held = true;
    held_q = dut->old_q_o;
    held_row = dut->old_row_o;
    held_col = dut->old_col_o;
    held_last = dut->old_last_o;
    held_sat = dut->old_saturated_o;
    held_err = dut->old_error_o;
  } else {
    held = false;
  }
}

static void tick() {
  dut->clk = 0;
  dut->eval();
  compare_low();
  dut->clk = 1;
  dut->eval();
  ++cycles;
}

static void reset() {
  dut->job_valid_i = 0;
  dut->cov_valid_i = 0;
  dut->q_ready_i = 0;
  dut->rst_n = 0;
  held = false;
  for (int i = 0; i < 4; ++i) tick();
  dut->rst_n = 1;
  tick();
}

struct Row { uint8_t row; uint16_t mask; };
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

static void drive_job_fields(const Job& j) {
  put_wide(dut->job_n0_i, j.n0);
  put_wide(dut->job_dndx_i, j.dx);
  put_wide(dut->job_dndy_i, j.dy);
  dut->job_area2_i = j.area;
  dut->job_min_x_i = static_cast<uint16_t>(j.min_x) & 0x0fff;
  dut->job_tile_x_i = static_cast<uint16_t>(j.tile_x) & 0x0fff;
  dut->job_tile_y_i = static_cast<uint16_t>(j.tile_y) & 0x0fff;
}

static uint64_t run_job(const Job& j, uint64_t seed) {
  drive_job_fields(j);
  dut->job_valid_i = 1;
  bool accepted = false;
  for (int i = 0; i < 2000 && !accepted; ++i) {
    dut->clk = 0;
    dut->eval();
    compare_low();
    const bool fire = dut->job_valid_i && dut->old_job_ready_o &&
                      dut->new_job_ready_o;
    dut->clk = 1;
    dut->eval();
    ++cycles;
    accepted = fire;
  }
  if (!accepted) fail("job did not accept");
  dut->job_valid_i = 0;
  // The source pins are not lifetime authorities after acceptance.
  put_wide(dut->job_dndx_i, j.dx ^ (i128{1} << 47));
  put_wide(dut->job_dndy_i, j.dy ^ (i128{1} << 23));
  dut->job_min_x_i ^= 0x07f;
  dut->job_tile_y_i ^= 0x155;

  std::mt19937_64 rng(seed);
  size_t next_row = 0;
  bool row_offered = false;
  uint64_t retired = 0;
  bool forced_hold = false;
  for (int guard = 0; guard < 500000; ++guard) {
    if (!row_offered && next_row < j.rows.size()) {
      dut->cov_valid_i = 1;
      dut->cov_row_i = j.rows[next_row].row;
      dut->cov_mask_i = j.rows[next_row].mask;
      dut->cov_last_i = next_row + 1 == j.rows.size();
      row_offered = true;
    }
    dut->q_ready_i = (rng() & 3u) != 0;
    dut->clk = 0;
    dut->eval();
    if (dut->old_q_valid_o && !forced_hold) {
      dut->q_ready_i = 0;
      dut->eval();
      forced_hold = true;
    }
    compare_low();
    const bool cov_fire = dut->cov_valid_i && dut->old_cov_ready_o &&
                          dut->new_cov_ready_o;
    const bool q_fire = dut->old_q_valid_o && dut->q_ready_i;
    dut->clk = 1;
    dut->eval();
    ++cycles;
    if (cov_fire) {
      ++next_row;
      row_offered = false;
      dut->cov_valid_i = 0;
    }
    if (q_fire) ++retired;
    dut->clk = 0;
    dut->eval();
    compare_low();
    if (next_row == j.rows.size() && !row_offered && dut->old_idle_o &&
        dut->new_idle_o && !dut->old_q_valid_o && !dut->new_q_valid_o) {
      dut->q_ready_i = 0;
      return retired;
    }
  }
  fail("job did not drain");
  return retired;
}

static void reset_during_setup(const Job& j, int clocks_after_accept) {
  drive_job_fields(j);
  dut->job_valid_i = 1;
  for (int guard = 0; guard < 1000; ++guard) {
    dut->clk = 0;
    dut->eval();
    compare_low();
    const bool fire = dut->old_job_ready_o && dut->new_job_ready_o;
    tick();
    if (fire) break;
  }
  dut->job_valid_i = 0;
  for (int i = 0; i < clocks_after_accept; ++i) tick();
  dut->rst_n = 0;
  tick();
  dut->rst_n = 1;
  tick();
  if (!dut->old_job_ready_o || !dut->new_job_ready_o ||
      !dut->old_idle_o || !dut->new_idle_o)
    fail("reset did not clear setup micro-work");
}

static void reset_after_gradient(const Job& j, int clocks_after_cov_ready) {
  drive_job_fields(j);
  dut->job_valid_i = 1;
  bool accepted = false;
  for (int guard = 0; guard < 1000 && !accepted; ++guard) {
    dut->clk = 0;
    dut->eval();
    compare_low();
    accepted = dut->old_job_ready_o && dut->new_job_ready_o;
    tick();
  }
  if (!accepted) fail("gradient-reset job did not accept");
  dut->job_valid_i = 0;
  dut->cov_valid_i = 0;

  bool gradient_ready = false;
  for (int guard = 0; guard < 2000 && !gradient_ready; ++guard) {
    dut->clk = 0;
    dut->eval();
    compare_low();
    gradient_ready = dut->old_cov_ready_o && dut->new_cov_ready_o;
    if (!gradient_ready) tick();
  }
  if (!gradient_ready) fail("gradient-reset probe did not reach coverage-ready");
  for (int i = 0; i < clocks_after_cov_ready; ++i) tick();

  dut->rst_n = 0;
  tick();
  dut->rst_n = 1;
  tick();
  if (!dut->old_job_ready_o || !dut->new_job_ready_o ||
      !dut->old_idle_o || !dut->new_idle_o)
    fail("reset did not clear offset micro-work");
}

static void hard_exit(int rc) {
  std::fflush(stdout);
  std::_Exit(rc);
}

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  dut = new Vtb_raster_attrgrad_dsp3_pair;
  dut->clk = 0;
  reset();

  const Job setup_probe{123, -733, 417, 997, -7, 8, 3, {{0, 0x8001}}};
  for (int phase = 0; phase < 8; ++phase) reset_during_setup(setup_probe, phase);
  for (int phase = 0; phase < 6; ++phase) reset_after_gradient(setup_probe, phase);

  std::vector<Job> jobs = {
      {-3, 6, 0, 10, 0, 2, 0, {{0, 0x8001}}},
      {4500, 901, -233, 997, -3, 5, -4,
       {{0, 0x8421}, {5, 0x1088}, {15, 0x9001}}},
      {-8100, -733, 417, 1024, -7, 8, 3,
       {{1, 0x00f3}, {7, 0x8101}, {13, 0x2222}}},
      {0, i128{1} << 40, 0, 1, 0, 1, 0, {{2, 0x0005}}},
      // Piece-boundary controls stay nonsaturating so the signed-middle and
      // narrow-high-shift mutants cannot hide behind the same s32 rail.
      {0, i128{1} << 47, 0, uint64_t{1} << 46, 1, 1, 0, {{0, 0x0011}}},
      {0, i128{1} << 48, 0, uint64_t{1} << 46, 1, 1, 0, {{0, 0x0101}}},
      {99, -17, 31, 0, 0, 4, -2, {{0, 0x0003}, {9, 0x8000}}},
      {(i128{1} << 95) - 1, (i128{1} << 71) - 1, -(i128{1} << 70),
       (uint64_t{1} << 47) - 1, -2048, 2047, -2048, {{15, 0xffff}}},
  };
  std::mt19937_64 rng(0xA773'2026'0915ULL);
  for (int i = 0; i < 24; ++i) {
    const i128 dx = static_cast<int64_t>(rng() % 2000001) - 1000000;
    const i128 dy = static_cast<int64_t>(rng() % 2000001) - 1000000;
    const i128 n0 = static_cast<i128>(static_cast<int64_t>(rng())) *
                    static_cast<i128>(static_cast<int64_t>(rng() | 1));
    const uint64_t area = 1 + (rng() % 1000000);
    const int min_x = static_cast<int>(rng() % 4096) - 2048;
    const int tile_x = static_cast<int>(rng() % 4096) - 2048;
    const int tile_y = static_cast<int>(rng() % 4096) - 2048;
    jobs.push_back(Job{n0, dx, dy, area, min_x, tile_x, tile_y,
                       {{0, static_cast<uint16_t>(rng() | 1)},
                        {7, static_cast<uint16_t>(rng() | 1)},
                        {15, static_cast<uint16_t>(rng() | 1)}}});
  }

  uint64_t pixels = 0;
  for (size_t i = 0; i < jobs.size(); ++i)
    pixels += run_job(jobs[i], 0xD500 + i);

  if (pixels == 0 || dut->old_pixels_o == 0 || dut->old_divides_o == 0 ||
      dut->old_saturations_o == 0 || dut->old_errors_o == 0)
    fail("differential detector coverage was vacuous");

#ifdef EXPECT_ATTR3_OFFSET_GUARD
  std::printf("FAIL: ATTR3 OFFSET GUARD MISSED\n");
  hard_exit(1);
#elif defined(EXPECT_ATTR3_MUTANT)
  std::printf("ATTR3 MUTANT %s: jobs=%zu pixels=%llu cycles=%llu mismatches=%d\n",
              failures != 0 ? "FIRED" : "MISSED", jobs.size(),
              static_cast<unsigned long long>(pixels),
              static_cast<unsigned long long>(cycles), failures);
  hard_exit(failures != 0 ? 0 : 1);
#else
  std::printf("ATTR3 %s: jobs=%zu pixels=%llu cycles=%llu\n",
              failures == 0 ? "PASS" : "FAIL", jobs.size(),
              static_cast<unsigned long long>(pixels),
              static_cast<unsigned long long>(cycles));
  hard_exit(failures == 0 ? 0 : 1);
#endif
}
