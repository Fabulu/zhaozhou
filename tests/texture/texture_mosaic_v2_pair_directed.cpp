// texture_mosaic_v2_pair_directed.cpp — cycle-exact legacy/V2 differential.
#include <cstdint>
#include <cstdio>

#include "verilated.h"
#include "Vtb_texture_mosaic_v2_pair.h"
#include "zhao_sim.hpp"

namespace {

uint32_t next(uint32_t* state) {
  *state = *state * 1664525u + 1013904223u;
  return *state;
}

void compare_cycle(Vtb_texture_mosaic_v2_pair& top, int* mismatches, int* valid_payload_checks,
                   int* idle_checks) {
  if (top.legacy_req_ready_o != top.v2_req_ready_o) ++*mismatches;
  if (top.legacy_pick_valid_o != top.v2_pick_valid_o) ++*mismatches;
  if (top.legacy_samples_o != top.v2_samples_o) ++*mismatches;

  // Payload is a ready/valid record and is compared on every cycle where either
  // implementation claims it valid.  Invalid stale bits are not protocol data.
  if (top.legacy_pick_valid_o || top.v2_pick_valid_o) {
    ++*valid_payload_checks;
    if (top.legacy_pick_tile_o != top.v2_pick_tile_o || top.legacy_pick_tx_o != top.v2_pick_tx_o ||
        top.legacy_pick_ty_o != top.v2_pick_ty_o ||
        top.legacy_pick_src_id_o != top.v2_pick_src_id_o)
      ++*mismatches;
  }

  ++*idle_checks;
  if (top.legacy_idle_o != top.v2_idle_o) ++*mismatches;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_texture_mosaic_v2_pair top;
  top.req_valid_i = 0;
  top.pick_ready_i = 0;
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

  uint32_t valid_rng = 0x13579BDFu;
  uint32_t ready_rng = 0x2468ACE1u;
  uint32_t data_rng = 0xC001D00Du;
  bool pending = false;
  int accepted = 0;
  int retired = 0;
  int input_gap_cycles = 0;
  int output_stall_cycles = 0;

  for (int cycle = 0; cycle < 5000; ++cycle) {
    if (!pending && cycle < 4000 && ((next(&valid_rng) >> 28) < 11)) {
      pending = true;
      top.req_u_i = next(&data_rng);
      top.req_v_i = next(&data_rng);
      top.req_mat_a_i = static_cast<uint8_t>(next(&data_rng));
      top.req_mat_b_i = static_cast<uint8_t>(next(&data_rng));
      top.req_weight_i = static_cast<uint8_t>(next(&data_rng));
      top.req_mosaic_i = static_cast<uint8_t>(next(&data_rng) & 1u);
      top.req_src_id_i = static_cast<uint16_t>(next(&data_rng));
    }
    top.req_valid_i = pending ? 1 : 0;
    top.pick_ready_i = ((next(&ready_rng) >> 27) < 21) ? 1 : 0;
    if (!top.req_valid_i) ++input_gap_cycles;

    top.eval();
    compare_cycle(top, &mismatches, &payload_checks, &idle_checks);
    const bool accept = top.req_valid_i && top.legacy_req_ready_o && top.v2_req_ready_o;
    const bool take = top.legacy_pick_valid_o && top.v2_pick_valid_o && top.pick_ready_i;
    if (top.legacy_pick_valid_o && !top.pick_ready_i) ++output_stall_cycles;
    if (accept) {
      pending = false;
      ++accepted;
    }
    if (take) ++retired;
    zhao::tick(top);
  }

  top.req_valid_i = 0;
  top.pick_ready_i = 1;
  for (int guard = 0; guard < 100 && (!top.legacy_idle_o || !top.v2_idle_o); ++guard) {
    top.eval();
    compare_cycle(top, &mismatches, &payload_checks, &idle_checks);
    if (top.legacy_pick_valid_o && top.v2_pick_valid_o) ++retired;
    zhao::tick(top);
  }
  top.eval();
  compare_cycle(top, &mismatches, &payload_checks, &idle_checks);

  zhao::check(mismatches == 0, "Mosaic V2 matches every legacy ready/valid/payload/counter cycle",
              0, mismatches);
  zhao::check(accepted > 1000 && accepted == retired,
              "Mosaic pair accepts and retires the same nontrivial stream", accepted, retired);
  zhao::check(input_gap_cycles > 0 && output_stall_cycles > 0,
              "Mosaic valid gaps and sink backpressure were independently exercised", 1,
              (input_gap_cycles > 0 && output_stall_cycles > 0) ? 1 : 0);
  zhao::check(payload_checks > accepted,
              "Mosaic payload compared on valid stall cycles as well as transfers", 1,
              payload_checks > accepted ? 1 : 0);
  zhao::check(idle_checks > 5000 && top.v2_idle_o,
              "Mosaic V2 idle was checked separately every cycle and at drain", 1,
              (idle_checks > 5000 && top.v2_idle_o) ? 1 : 0);
  return zhao::report_and_exit("texture_mosaic_v2_pair_directed");
}
