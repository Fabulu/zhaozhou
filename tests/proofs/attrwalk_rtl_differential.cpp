// attrwalk_rtl_differential.cpp — RASTER.ATTRWALK, the divider-free row
// walker, differentialed pixel-for-pixel against the repo's ACTUAL sources of
// truth. No restated oracles (see attrstep_qr_differential.cpp for why that
// rule exists in this chain):
//
//   * Vattrwalk_away — the walker at TIE_TOWARD_POSITIVE = 0 (default):
//     every emitted pixel must equal the ACTUAL verilated zhao_raster_attrdiv
//     fed the exact numerator. Ties of both signs, sign crossings mid-row,
//     sparse masks and backpressure included.
//   * Vattrwalk_pos — the walker at TIE_TOWARD_POSITIVE = 1: every emitted
//     pixel must equal the ACTUAL zref::render::div_rhu_s128, compiled from
//     reference/src/zrender/rast.cpp in this build. One parameter reaches the
//     reference law exactly — that is the demonstration that the architecture
//     is law-neutral and the tie is the owner's single ratification point.
//   * the range detector is FIRED with legal stimulus: a tile whose covered
//     pixels' quotients exceed s32 must raise q_error_o and move
//     range_errs_o. A detector never seen to fire is not a detector.
//
// The walker takes three Euclidean pairs per job. Here the seed stage is
// played by exact __int128 division; in silicon it is the seed service (see
// reports/ATTRSTEP_QR_REARCHITECTURE_20260909.md for the budget).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include "internal.hpp"  // zref::render::div_rhu_s128 — the ACTUAL declaration

#include "Vattrwalk_away.h"
#include "Vattrwalk_pos.h"
#include "Vzhao_raster_attrdiv.h"
#include "verilated.h"

using i128 = __int128;

struct Pair {
  i128 q;
  i128 r;
};

static Pair edecomp(i128 n, i128 A) {
  i128 q = n / A;
  i128 r = n - q * A;
  if (r < 0) {
    --q;
    r += A;
  }
  return Pair{q, r};
}

// ---------------------------------------------------------------------------
// the actual RTL divider (truth for the away-from-zero build)
// ---------------------------------------------------------------------------
struct RtlDiv {
  Vzhao_raster_attrdiv* m;
  RtlDiv() {
    m = new Vzhao_raster_attrdiv;
    m->clk = 0;
    m->rst_n = 0;
    m->v_valid_i = 0;
    m->r_ready_i = 0;
    for (int i = 0; i < 4; ++i) tick();
    m->rst_n = 1;
    tick();
  }
  ~RtlDiv() {
    m->final();
    delete m;
  }
  void tick() {
    m->clk = 0;
    m->eval();
    m->clk = 1;
    m->eval();
  }
  int32_t divide(i128 n, uint64_t area, bool* ovf) {
    const unsigned __int128 un = static_cast<unsigned __int128>(n);
    m->num_i[0] = static_cast<uint32_t>(un);
    m->num_i[1] = static_cast<uint32_t>(un >> 32);
    m->num_i[2] = static_cast<uint32_t>(un >> 64);
    m->area_i = area;
    m->v_valid_i = 1;
    for (int g = 0;; ++g) {
      m->eval();
      const bool fire = m->v_valid_i && m->v_ready_o;
      tick();
      if (fire) break;
      if (g > 200) std::exit(2);
    }
    m->v_valid_i = 0;
    m->r_ready_i = 1;
    int32_t q = 0;
    for (int g = 0;; ++g) {
      m->eval();
      if (m->r_valid_o) {
        q = static_cast<int32_t>(m->q_o);
        *ovf = m->q_overflow_o != 0;
        tick();
        break;
      }
      tick();
      if (g > 200) std::exit(2);
    }
    m->r_ready_i = 0;
    return q;
  }
};

// ---------------------------------------------------------------------------
// the walker driver, templated over the two tie-law builds
// ---------------------------------------------------------------------------
struct EmittedPix {
  int row, col;
  int32_t q;
  bool err;
};

template <typename V>
struct Walker {
  V* m;
  uint64_t cycles = 0;
  std::mt19937_64 stall{12345};

  Walker() {
    m = new V;
    m->clk = 0;
    m->rst_n = 0;
    m->job_valid_i = 0;
    m->cov_valid_i = 0;
    m->q_ready_i = 0;
    for (int i = 0; i < 4; ++i) tick();
    m->rst_n = 1;
    tick();
  }
  ~Walker() {
    m->final();
    delete m;
  }
  void tick() {
    m->clk = 0;
    m->eval();
    m->clk = 1;
    m->eval();
    ++cycles;
  }
  static void put96(VlWide<3>& w, i128 v) {
    const unsigned __int128 u = static_cast<unsigned __int128>(v);
    w[0] = static_cast<uint32_t>(u);
    w[1] = static_cast<uint32_t>(u >> 32);
    w[2] = static_cast<uint32_t>(u >> 64);
  }

  // Run one job: pairs in, coverage rows in, pixels out (with random
  // backpressure so a stallable walker is actually stalled).
  std::vector<EmittedPix> run(const Pair& p0, const Pair& px, const Pair& py,
                              uint64_t area, const std::vector<std::pair<int, uint16_t>>& rows) {
    put96(m->job_q0_i, p0.q);
    m->job_r0_i = static_cast<uint64_t>(p0.r);
    put96(m->job_dqx_i, px.q);
    m->job_drx_i = static_cast<uint64_t>(px.r);
    put96(m->job_dqy_i, py.q);
    m->job_dry_i = static_cast<uint64_t>(py.r);
    m->job_area_i = area;
    m->job_valid_i = 1;
    for (int g = 0;; ++g) {
      m->eval();
      const bool fire = m->job_valid_i && m->job_ready_o;
      tick();
      if (fire) break;
      if (g > 500) {
        std::printf("FATAL: walker never took the job\n");
        std::exit(2);
      }
    }
    m->job_valid_i = 0;

    std::vector<EmittedPix> out;
    size_t nrow = 0;
    bool row_offered = false;
    for (int g = 0; g < 200000; ++g) {
      // offer coverage
      if (!row_offered && nrow < rows.size()) {
        m->cov_valid_i = 1;
        m->cov_row_i = rows[nrow].first;
        m->cov_mask_i = rows[nrow].second;
        m->cov_last_i = (nrow + 1 == rows.size());
        row_offered = true;
      }
      // random backpressure
      m->q_ready_i = (stall() & 3) != 0;
      m->eval();
      const bool cov_fire = m->cov_valid_i && m->cov_ready_o;
      const bool pix_fire = m->q_valid_o && m->q_ready_i;
      EmittedPix p{};
      bool got_last = false;
      if (pix_fire) {
        p.row = m->q_row_o;
        p.col = m->q_col_o;
        p.q = static_cast<int32_t>(m->q_o);
        p.err = m->q_error_o != 0;
        got_last = m->q_last_o != 0;
      }
      tick();
      if (cov_fire) {
        ++nrow;
        row_offered = false;
        m->cov_valid_i = 0;
      }
      if (pix_fire) {
        out.push_back(p);
        if (got_last) {
          m->q_ready_i = 0;
          m->cov_valid_i = 0;
          return out;
        }
      }
    }
    std::printf("FATAL: walker job never finished\n");
    std::exit(2);
  }
};

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Walker<Vattrwalk_away> w_away;
  Walker<Vattrwalk_pos> w_pos;
  RtlDiv div;
  std::mt19937_64 rng(0xA77'2026'0909ULL);
  int fails = 0;

  long checked_px = 0, away_mismatch = 0, pos_mismatch = 0;
  long ties_pos = 0, ties_neg = 0, crossings = 0, laws_differ_px = 0;
  long rtl_divides = 0;

  auto run_tile = [&](i128 n_base, i128 dndx, i128 dndy, uint64_t area,
                      const std::vector<std::pair<int, uint16_t>>& rows) {
    const i128 A = static_cast<i128>(area);
    const Pair p0 = edecomp(n_base, A);
    const Pair px = edecomp(dndx, A);
    const Pair py = edecomp(dndy, A);
    auto got_away = w_away.run(p0, px, py, area, rows);
    auto got_pos = w_pos.run(p0, px, py, area, rows);
    // expected emission order: rows as offered, covered cols ascending
    size_t k = 0;
    for (const auto& rw : rows) {
      const i128 n_row = n_base + static_cast<i128>(rw.first) * dndy;
      bool was_neg = n_row < 0;
      for (int x = 0; x < 16; ++x) {
        const i128 n = n_row + static_cast<i128>(x) * dndx;
        if ((n < 0) != was_neg) {
          ++crossings;
          was_neg = n < 0;
        }
        if (!((rw.second >> x) & 1)) continue;
        const Pair p = edecomp(n, A);
        if (2 * p.r == A) {
          if (p.q >= 0) ++ties_pos;
          else ++ties_neg;
        }
        bool ovf = false;
        const int32_t want_away = div.divide(n, area, &ovf);
        ++rtl_divides;
        const int32_t want_pos = zref::render::div_rhu_s128(n, A);
        if (want_away != want_pos) ++laws_differ_px;
        ++checked_px;
        if (k >= got_away.size() || k >= got_pos.size()) {
          ++away_mismatch;
          ++k;
          continue;
        }
        const EmittedPix& ga = got_away[k];
        const EmittedPix& gp = got_pos[k];
        if (ga.row != rw.first || ga.col != x || ga.err || ovf || ga.q != want_away) {
          ++away_mismatch;
          if (away_mismatch <= 5)
            std::printf("  AWAY mismatch (%d,%d): walker=%ld err=%d attrdiv=%ld ovf=%d\n",
                        rw.first, x, (long)ga.q, ga.err ? 1 : 0, (long)want_away,
                        ovf ? 1 : 0);
        }
        if (gp.row != rw.first || gp.col != x || gp.err || gp.q != want_pos) {
          ++pos_mismatch;
          if (pos_mismatch <= 5)
            std::printf("  POS mismatch (%d,%d): walker=%ld err=%d zref=%ld\n",
                        rw.first, x, (long)gp.q, gp.err ? 1 : 0, (long)want_pos);
        }
        ++k;
      }
    }
    if (k != got_away.size() || k != got_pos.size()) {
      std::printf("  PIXEL COUNT wrong: want %zu, away %zu, pos %zu\n", k,
                  got_away.size(), got_pos.size());
      ++away_mismatch;
    }
  };

  std::printf("== walker vs ACTUAL attrdiv (away build) and ACTUAL zref (pos build) ==\n");
  auto full = [] {
    std::vector<std::pair<int, uint16_t>> r;
    for (int y = 0; y < 16; ++y) r.push_back({y, 0xFFFF});
    return r;
  }();

  // directed: zero crossing mid-row, ties of both signs woven through the walk
  run_tile(-7 * 1000 - 500, 1000, -3000, 2000, full);
  run_tile(128, 256, 256, 256, full);              // a tie at every pixel
  // negative ties in-walk: N = -2304 + 512(x+y), A = 512 -> r = 256 at EVERY
  // pixel, q < 0 for x+y < 5. The first version of this line used a base that
  // was 384 mod 512 — never a tie — and the anti-vacuity gate below refused
  // the run. That is the gate doing its one job.
  run_tile(-2304, 512, 512, 512, full);
  {                                                // near the numerator ceiling
    const uint64_t A = (1ULL << 46) - 4;
    const i128 base = -(static_cast<i128>(1) << 30) * static_cast<i128>(A) + 12345;
    run_tile(base, (static_cast<i128>(1) << 45), (static_cast<i128>(1) << 44) + 7, A, full);
  }
  // random tiles with random sparse coverage
  for (int t = 0; t < 16; ++t) {
    const int mag = 8 + static_cast<int>(rng() % 38);
    const uint64_t A = (rng() % (1ULL << mag)) + 2;
    const i128 base = static_cast<i128>(static_cast<int64_t>(
                          static_cast<int32_t>(rng()) / 4)) *
                          static_cast<i128>(A) +
                      static_cast<i128>(rng() % A);
    const i128 dx =
        static_cast<i128>(static_cast<int64_t>(rng() % (A * 3 + 7))) - static_cast<i128>(A);
    const i128 dy =
        static_cast<i128>(static_cast<int64_t>(rng() % (A * 3 + 7))) - static_cast<i128>(A);
    std::vector<std::pair<int, uint16_t>> rows;
    for (int y = 0; y < 16; ++y) {
      const uint16_t mask = static_cast<uint16_t>(rng());
      if (mask) rows.push_back({y, mask});
    }
    if (rows.empty()) rows.push_back({3, 0x0010});
    run_tile(base, dx, dy, A, rows);
  }

  std::printf("   %ld covered pixels checked (%ld RTL divides for truth)\n", checked_px,
              rtl_divides);
  std::printf("   away-build vs attrdiv mismatches %ld (MUST be 0)\n", away_mismatch);
  std::printf("   pos-build  vs zref    mismatches %ld (MUST be 0)\n", pos_mismatch);
  std::printf("   ties %ld/%ld, crossings %ld (all MUST be > 0)\n", ties_pos, ties_neg,
              crossings);
  std::printf("   pixels where the two LAWS differ: %ld (MUST be > 0, or ties were vacuous)\n",
              laws_differ_px);
  if (away_mismatch || pos_mismatch || !ties_pos || !ties_neg || !crossings ||
      !laws_differ_px)
    ++fails;

  // ---- fire the range detector with legal stimulus -------------------------
  std::printf("== the range detector fires ==\n");
  {
    const uint64_t area = 1000;
    const i128 A = static_cast<i128>(area);
    // covered pixels sit at quotients around +2^31 + few: out of s32 range
    const i128 base = (static_cast<i128>(INT32_MAX) + 3) * A + 7;
    const Pair p0 = edecomp(base, A);
    const Pair px = edecomp(static_cast<i128>(A), A);      // +1 quotient per px
    const Pair py = edecomp(static_cast<i128>(0), A);
    std::vector<std::pair<int, uint16_t>> rows{{0, 0x000F}};
    const uint32_t errs_before = w_away.m->range_errs_o;
    auto got = w_away.run(p0, px, py, area, rows);
    const uint32_t errs_after = w_away.m->range_errs_o;
    long flagged = 0;
    for (const auto& p : got) flagged += p.err ? 1 : 0;
    std::printf("   4 out-of-range pixels: %ld flagged, range_errs_o %u -> %u\n", flagged,
                errs_before, errs_after);
    if (flagged != 4 || errs_after != errs_before + 4) ++fails;
  }

  std::printf("\nwalker cycles: away %llu, pos %llu\n",
              (unsigned long long)w_away.cycles, (unsigned long long)w_pos.cycles);
  if (fails == 0) {
    std::printf("ALL SECTIONS PASS\n");
    return 0;
  }
  std::printf("FAILURES: %d\n", fails);
  return 1;
}
