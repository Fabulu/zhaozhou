// texture_bilerp_lane_v2_pair_directed.cpp — cycle-exact legacy/V2 differential.
#include <cstdint>
#include <cstdio>

#include "verilated.h"
#include "Vtb_texture_bilerp_lane_v2_pair.h"
#include "zhao_sim.hpp"

namespace {

uint32_t next(uint32_t* state) {
  *state = *state * 1103515245u + 12345u;
  return *state;
}

void compare_cycle(Vtb_texture_bilerp_lane_v2_pair& top, int* mismatches, int* payload_checks,
                   int* idle_checks) {
  if (top.legacy_job_ready_o != top.v2_job_ready_o) ++*mismatches;
  if (top.legacy_out_valid_o != top.v2_out_valid_o) ++*mismatches;
  if (top.legacy_jobs_o != top.v2_jobs_o) ++*mismatches;
  if (top.legacy_occupancy_o != top.v2_occupancy_o) ++*mismatches;

  if (top.legacy_out_valid_o || top.v2_out_valid_o) {
    ++*payload_checks;
    if (top.legacy_out_o != top.v2_out_o || top.legacy_out_tok_o != top.v2_out_tok_o ||
        top.legacy_out_chan_o != top.v2_out_chan_o)
      ++*mismatches;
  }

  ++*idle_checks;
  const bool expected_idle = top.legacy_occupancy_o == 0;
  if (static_cast<bool>(top.v2_idle_o) != expected_idle) ++*mismatches;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_texture_bilerp_lane_v2_pair top;
  top.job_valid_i = 0;
  top.out_ready_i = 0;
  top.rst_n = 0;

  int mismatches = 0;
  int payload_checks = 0;
  int idle_checks = 0;
  for (int i = 0; i < 8; ++i) {
    top.eval();
    compare_cycle(top, &mismatches, &payload_checks, &idle_checks);
    zhao::tick(top);
  }
  top.rst_n = 1;

  uint32_t valid_rng = 0x9E3779B9u;
  uint32_t ready_rng = 0x7F4A7C15u;
  uint32_t data_rng = 0xDEADBEEFu;
  bool pending = false;
  int accepted = 0;
  int retired = 0;
  int valid_gap_cycles = 0;
  int stalled_valid_cycles = 0;

  for (int cycle = 0; cycle < 6000; ++cycle) {
    if (!pending && cycle < 5000 && ((next(&valid_rng) >> 28) < 12)) {
      pending = true;
      top.t00_i = static_cast<uint8_t>(next(&data_rng));
      top.t10_i = static_cast<uint8_t>(next(&data_rng));
      top.t01_i = static_cast<uint8_t>(next(&data_rng));
      top.t11_i = static_cast<uint8_t>(next(&data_rng));
      top.fu_i = static_cast<uint8_t>(next(&data_rng));
      top.fv_i = static_cast<uint8_t>(next(&data_rng));
      top.tok_i = next(&data_rng) & 0x3FFFFu;
      top.chan_i = next(&data_rng) & 3u;
    }
    top.job_valid_i = pending ? 1 : 0;
    top.out_ready_i = ((next(&ready_rng) >> 27) < 20) ? 1 : 0;
    if (!top.job_valid_i) ++valid_gap_cycles;

    top.eval();
    compare_cycle(top, &mismatches, &payload_checks, &idle_checks);
    const bool accept = top.job_valid_i && top.legacy_job_ready_o && top.v2_job_ready_o;
    const bool take = top.legacy_out_valid_o && top.v2_out_valid_o && top.out_ready_i;
    if (top.legacy_out_valid_o && !top.out_ready_i) ++stalled_valid_cycles;
    if (accept) {
      pending = false;
      ++accepted;
    }
    if (take) ++retired;
    zhao::tick(top);
  }

  top.job_valid_i = 0;
  top.out_ready_i = 1;
  for (int guard = 0; guard < 100 && (top.legacy_occupancy_o != 0 || !top.v2_idle_o); ++guard) {
    top.eval();
    compare_cycle(top, &mismatches, &payload_checks, &idle_checks);
    if (top.legacy_out_valid_o && top.v2_out_valid_o) ++retired;
    zhao::tick(top);
  }
  top.eval();
  compare_cycle(top, &mismatches, &payload_checks, &idle_checks);

  zhao::check(mismatches == 0, "bilerp V2 matches every legacy ready/valid/payload/counter cycle",
              0, mismatches);
  zhao::check(accepted > 1500 && accepted == retired,
              "bilerp pair accepts and retires the same nontrivial stream", accepted, retired);
  zhao::check(valid_gap_cycles > 0 && stalled_valid_cycles > 0,
              "bilerp valid gaps and output backpressure were independently exercised", 1,
              (valid_gap_cycles > 0 && stalled_valid_cycles > 0) ? 1 : 0);
  zhao::check(payload_checks > accepted, "bilerp payload compared through held-valid stall cycles",
              1, payload_checks > accepted ? 1 : 0);
  zhao::check(idle_checks > 6000 && top.v2_idle_o && top.legacy_occupancy_o == 0,
              "bilerp V2 idle separately equals zero legacy occupancy every cycle", 1,
              (idle_checks > 6000 && top.v2_idle_o) ? 1 : 0);
  return zhao::report_and_exit("texture_bilerp_lane_v2_pair_directed");
}
