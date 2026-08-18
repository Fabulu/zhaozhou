// raster_dev.hpp — Verilator driver for the RASTER.EDGEWALK differential
// tests (design/contracts/RASTER.EDGEWALK.md).
//
// The oracle is zref::EdgeWalk (reference/include/zref/zref_edgewalk.hpp) —
// the ledger's declared reference_model, which is itself a view onto the
// frozen zref::render::raster_tri and NOT a second implementation of the §8
// fill law. This file therefore contains no fill rule at all: it drives the
// RTL and compares masks.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_raster_edgewalk.h"

#include "zhao_sim.hpp"
#include "zref/zref_edgewalk.hpp"

namespace zhao_raster {

using Tri = zref::EdgeWalk::Tri;
using TileCov = zref::EdgeWalk::Cov;

inline constexpr int kTile = zref::EdgeWalk::kTile;
inline constexpr int32_t kGuard = zref::EdgeWalk::kGuard;

// the oracle, by its ledger name
inline TileCov oracle_cover(const Tri& t, int32_t tile_x, int32_t tile_y) {
  return zref::EdgeWalk::tile(t, tile_x, tile_y);
}

// ---------------------------------------------------------------- device ---
// Drives the Verilated zhao_raster_edgewalk through one job and returns its
// coverage. `stall_seed` != 0 gates cov_ready_i with a PCG bit stream — the
// backpressure lane; 0 means "always ready".
class RtlDev {
 public:
  // The Verilated model is held BY VALUE, not new/delete: the device owns no
  // dynamic resource, so there is no copy/ownership hazard to get wrong.
  RtlDev() { reset(); }
  ~RtlDev() { top_.final(); }
  RtlDev(const RtlDev&) = delete;
  RtlDev& operator=(const RtlDev&) = delete;

  void reset() {
    top_.rst_n = 0;
    park();
    top_.eval();
    for (int i = 0; i < 2; ++i) edge();
    top_.rst_n = 1;
    top_.eval();
  }

  // Runs one job. Any protocol violation is appended to *err (empty = clean).
  TileCov run(const Tri& t, int32_t tile_x, int32_t tile_y, uint16_t src_id, uint32_t stall_seed,
              std::string* err) {
    TileCov out;
    park();

    // ---- offer the job (ready/valid) ------------------------------------
    top_.job_valid_i = 1;
    top_.job_ax_i = mask21(t.ax);
    top_.job_ay_i = mask21(t.ay);
    top_.job_bx_i = mask21(t.bx);
    top_.job_by_i = mask21(t.by);
    top_.job_cx_i = mask21(t.cx);
    top_.job_cy_i = mask21(t.cy);
    top_.job_tile_x_i = mask12(tile_x);
    top_.job_tile_y_i = mask12(tile_y);
    top_.job_src_id_i = src_id;
    top_.eval();
    for (int guard = 0; !top_.job_ready_o; ++guard) {
      if (guard > 64) {
        add(err, "job_ready_o never asserted");
        return out;
      }
      edge();
      top_.eval();
    }
    edge();  // the accepting edge
    park();
    top_.eval();

    // ---- collect the coverage beats -------------------------------------
    uint32_t rng = stall_seed;
    bool held = false;       // previous cycle stalled a beat
    uint32_t held_pack = 0;  // {last, row, mask} of the stalled beat
    bool seen_last = false;
    bool seen_row[kTile] = {};

    for (int guard = 0;; ++guard) {
      if (guard > 512) {
        add(err, "job never completed");
        return out;
      }
      const bool rdy = (stall_seed == 0) || ((next(&rng) & 3u) != 0u);
      top_.cov_ready_i = rdy ? 1 : 0;
      top_.eval();

      if (top_.cov_valid_o) {
        const uint32_t pack = (static_cast<uint32_t>(top_.cov_last_o) << 20) |
                              (static_cast<uint32_t>(top_.cov_row_o) << 16) |
                              static_cast<uint32_t>(top_.cov_mask_o);
        if (held && pack != held_pack) add(err, "stalled beat changed while cov_ready_i was low");
        if (top_.cov_src_id_o != src_id) add(err, "cov_src_id_o mismatch");
        if (seen_last) add(err, "beat emitted after cov_last_o");
        if (rdy) {
          const int r = top_.cov_row_o;
          if (seen_row[r]) add(err, "row emitted twice");
          seen_row[r] = true;
          out.row[r] = top_.cov_mask_o;
          if (top_.cov_mask_o == 0) add(err, "empty row emitted");
          if (top_.cov_last_o) seen_last = true;
          held = false;
        } else {
          held = true;
          held_pack = pack;
        }
      } else {
        if (held) add(err, "cov_valid_o dropped while a beat was stalled");
        held = false;
      }

      // one job in flight: the block must not offer to take another until
      // it is done, or a producer could restart it mid-walk
      if (top_.job_ready_o) add(err, "job_ready_o asserted while a job is in flight");

      edge();
      if (top_.job_done_o) {
        out.count = top_.cov_count_o;
        out.degenerate = top_.job_degenerate_o != 0;
        break;
      }
    }

    if (out.count != 0 && !seen_last) add(err, "job finished without cov_last_o");
    uint32_t popcount = 0;
    for (int y = 0; y < kTile; ++y)
      for (int x = 0; x < kTile; ++x) popcount += (out.row[y] >> x) & 1u;
    if (popcount != out.count) add(err, "cov_count_o disagrees with the emitted masks");

    park();
    top_.eval();
    return out;
  }

 private:
  static uint32_t mask21(int32_t v) { return static_cast<uint32_t>(v) & 0x1FFFFFu; }
  static uint32_t mask12(int32_t v) { return static_cast<uint32_t>(v) & 0xFFFu; }
  static uint32_t next(uint32_t* s) {
    *s = (*s) * 747796405u + 2891336453u;
    const uint32_t w = ((*s >> ((*s >> 28) + 4)) ^ *s) * 277803737u;
    return (w >> 22) ^ w;
  }
  static void add(std::string* err, const char* what) {
    if (err->empty()) *err = what;
  }

  void park() {
    top_.job_valid_i = 0;
    top_.cov_ready_i = 1;
  }
  void edge() {
    top_.clk = 0;
    top_.eval();
    top_.clk = 1;
    top_.eval();
    top_.clk = 0;
    top_.eval();
  }

  Vzhao_raster_edgewalk top_;
};

// Pretty-print a mismatch (charter §29-17 minimal failing vector body).
inline std::string describe(const Tri& t, int32_t tx, int32_t ty, const TileCov& want,
                            const TileCov& got) {
  char buf[256];
  std::snprintf(buf, sizeof(buf), "tri (%d,%d)(%d,%d)(%d,%d) tile (%d,%d)", t.ax, t.ay, t.bx, t.by,
                t.cx, t.cy, tx, ty);
  std::string s(buf);
  for (int y = 0; y < kTile; ++y) {
    if (want.row[y] == got.row[y]) continue;
    std::snprintf(buf, sizeof(buf), "\n  row %2d: oracle %04X rtl %04X", y, want.row[y],
                  got.row[y]);
    s += buf;
  }
  std::snprintf(buf, sizeof(buf), "\n  count oracle %u rtl %u; degenerate %d/%d", want.count,
                got.count, static_cast<int>(want.degenerate), static_cast<int>(got.degenerate));
  s += buf;
  return s;
}

inline std::vector<uint8_t> serialize(const Tri& t, int32_t tx, int32_t ty) {
  std::vector<uint8_t> v;
  auto put = [&v](int32_t x) {
    for (int i = 0; i < 4; ++i)
      v.push_back(static_cast<uint8_t>(static_cast<uint32_t>(x) >> (8 * i)));
  };
  put(t.ax);
  put(t.ay);
  put(t.bx);
  put(t.by);
  put(t.cx);
  put(t.cy);
  put(tx);
  put(ty);
  return v;
}

}  // namespace zhao_raster
