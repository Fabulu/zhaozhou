#include "Vtb_packetb_observation_successors.h"
#include "verilated.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using Top = Vtb_packetb_observation_successors;
int g_checks = 0;
int g_failures = 0;

void check(bool condition, const char* what, uint64_t expected, uint64_t actual) {
  ++g_checks;
  if (condition) return;
  ++g_failures;
  std::printf("FAIL: %s: expected %llu got %llu\n", what,
              static_cast<unsigned long long>(expected),
              static_cast<unsigned long long>(actual));
}

void tick(Top& d) {
  d.clk = 0;
  d.eval();
  d.clk = 1;
  d.eval();
  d.clk = 0;
  d.eval();
}

uint32_t mix32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7FEB352Du;
  x ^= x >> 15;
  x *= 0x846CA68Bu;
  x ^= x >> 16;
  return x;
}

uint32_t rcp_denominator(uint32_t i) {
  if ((i % 37u) == 0u) return 0;
  if (i < 24u) return uint32_t{1} << i;
  return mix32(i + 0x52435034u) & 0x00FFFFFFu;
}

uint16_t rcp_token14(uint32_t i) {
  const uint16_t high6 = static_cast<uint16_t>(((i * 29u) % 63u) + 1u);
  return static_cast<uint16_t>((high6 << 8) | (i & 0xFFu));
}

uint16_t persp_tag14(uint32_t i) {
  const uint16_t high6 = static_cast<uint16_t>(((i * 43u + 7u) % 63u) + 1u);
  return static_cast<uint16_t>((high6 << 8) | ((i * 73u) & 0xFFu));
}

void drive_persp(Top& d, uint32_t i) {
  const uint32_t a = mix32(i + 0x50555350u);
  const uint32_t b = mix32(i + 0x56415849u);
  d.persp_u_over_w_i = a;
  d.persp_v_over_w_i = ~b + 1u;
  d.persp_mant_i = 0x00800000u | (mix32(i + 9u) & 0x007FFFFFu);
  d.persp_k_i = i & 63u;
  d.persp_depth_zero_i = ((i % 19u) == 0u);
  d.persp_tag_i = persp_tag14(i);
}

bool same_rcp_cycle(const Top& d) {
  return d.rcp_v3_ready_o == d.rcp_v4_ready_o &&
         d.rcp_v3_valid_o == d.rcp_v4_valid_o &&
         d.rcp_v3_r_o == d.rcp_v4_r_o &&
         d.rcp_v3_k_o == d.rcp_v4_k_o &&
         d.rcp_v3_zero_o == d.rcp_v4_zero_o &&
         d.rcp_v3_tok_o == d.rcp_v4_tok_o &&
         d.rcp_v3_accepted_o == d.rcp_v4_accepted_o &&
         d.rcp_v3_completed_o == d.rcp_v4_completed_o &&
         d.rcp_v3_mul_jobs_o == d.rcp_v4_mul_jobs_o &&
         d.rcp_v3_zero_jobs_o == d.rcp_v4_zero_jobs_o &&
         d.rcp_v3_phase_jobs_o == d.rcp_v4_phase_jobs_o &&
         d.rcp_v3_negcorr_jobs_o == d.rcp_v4_negcorr_jobs_o &&
         d.rcp_v3_occupancy_o == d.rcp_v4_occupancy_o &&
         d.rcp_v3_qerr_o == d.rcp_v4_qerr_o;
}

bool same_persp_cycle(const Top& d) {
  return d.persp_old_ready_o == d.persp_v2_ready_o &&
         d.persp_old_valid_o == d.persp_v2_valid_o &&
         d.persp_old_u_o == d.persp_v2_u_o &&
         d.persp_old_v_o == d.persp_v2_v_o &&
         d.persp_old_tag_o == d.persp_v2_tag_o &&
         d.persp_old_sat_o == d.persp_v2_sat_o &&
         d.persp_old_depth_zero_o == d.persp_v2_depth_zero_o &&
         d.persp_old_fragments_o == d.persp_v2_fragments_o &&
         d.persp_old_products_o == d.persp_v2_products_o &&
         d.persp_old_zero_products_o == d.persp_v2_zero_products_o &&
         d.persp_old_occupancy_o == d.persp_v2_occupancy_o;
}

}  // namespace

double sc_time_stamp() { return 0.0; }

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Top d;
  d.clk = 0;
  d.rst_n = 0;
  d.rcp_valid_i = 0;
  d.rcp_out_ready_i = 0;
  d.persp_valid_i = 0;
  d.persp_out_ready_i = 0;
  for (int i = 0; i < 4; ++i) tick(d);
  d.rst_n = 1;
  tick(d);
  d.eval();

  check(d.rcp_v4_idle_o && d.rcp_v4_occupancy_o == 0,
        "RCP V4 starts idle with no owned reciprocal", 1,
        d.rcp_v4_idle_o && d.rcp_v4_occupancy_o == 0);
  check(d.persp_v2_idle_o && d.persp_v2_occupancy_o == 0,
        "PERSPUV V2 starts idle with no owned pair", 1,
        d.persp_v2_idle_o && d.persp_v2_occupancy_o == 0);

  constexpr uint32_t kJobs = 512;
  uint32_t rcp_issued = 0;
  uint32_t rcp_retired = 0;
  uint32_t persp_issued = 0;
  uint32_t persp_retired = 0;
  uint32_t expected_rcp_zero = 0;
  uint32_t expected_persp_zero = 0;
  bool rcp_token_live[1u << 14] = {};
  bool persp_tag_live[1u << 14] = {};
  uint32_t rcp_token_roundtrip_errors = 0;
  uint32_t persp_tag_roundtrip_errors = 0;
  uint32_t rcp_high_identity_accepts = 0;
  uint32_t persp_high_identity_accepts = 0;
  uint32_t rcp_cycle_mismatches = 0;
  uint32_t persp_cycle_mismatches = 0;
  uint32_t rcp_occupancy_mismatches = 0;
  uint32_t persp_occupancy_mismatches = 0;
  uint32_t rcp_idle_mismatches = 0;
  uint32_t persp_idle_mismatches = 0;
  int32_t rcp_obligations = 0;
  int32_t persp_obligations = 0;
  uint32_t rcp_internal_obligation_cycles = 0;
  uint32_t rcp_held_output_cycles = 0;
  uint32_t persp_internal_obligation_cycles = 0;
  uint32_t persp_held_output_cycles = 0;
  uint32_t rcp_drained_cycles = 0;
  uint32_t persp_drained_cycles = 0;
  uint32_t rng = 0x13579BDFu;
  int elapsed = 0;

  for (; elapsed < 300000 && (rcp_retired < kJobs || persp_retired < kJobs); ++elapsed) {
    rng = rng * 1664525u + 1013904223u;

    d.rcp_valid_i = rcp_issued < kJobs;
    if (rcp_issued < kJobs) {
      d.rcp_d_i = rcp_denominator(rcp_issued);
      d.rcp_tok_i = rcp_token14(rcp_issued);
    }
    // Fill all contexts and hold a genuine completed result before releasing
    // into independently varying stalls.
    d.rcp_out_ready_i = (elapsed >= 240) && (((rng >> 3) & 7u) != 0u);

    d.persp_valid_i = persp_issued < kJobs;
    if (persp_issued < kJobs) drive_persp(d, persp_issued);
    d.persp_out_ready_i = (elapsed >= 120) && (((rng >> 17) & 3u) != 0u);

    d.eval();

    if (!same_rcp_cycle(d)) ++rcp_cycle_mismatches;
    if (!same_persp_cycle(d)) ++persp_cycle_mismatches;

    // Independent lifecycle oracles: these counts are updated only from the
    // successor's real ingress/egress handshakes below.  Neither DUT occupancy
    // nor DUT idle participates in their construction.
    if (rcp_obligations < 0 ||
        d.rcp_v4_occupancy_o != static_cast<uint32_t>(rcp_obligations))
      ++rcp_occupancy_mismatches;
    const bool rcp_should_idle = rcp_obligations == 0;
    if (static_cast<bool>(d.rcp_v4_idle_o) != rcp_should_idle) ++rcp_idle_mismatches;
    if (rcp_should_idle) {
      ++rcp_drained_cycles;
    } else if (d.rcp_v4_valid_o && !d.rcp_out_ready_i) {
      ++rcp_held_output_cycles;
    } else {
      ++rcp_internal_obligation_cycles;
    }

    if (persp_obligations < 0 ||
        d.persp_v2_occupancy_o != static_cast<uint32_t>(persp_obligations))
      ++persp_occupancy_mismatches;
    const bool persp_should_idle = persp_obligations == 0;
    if (static_cast<bool>(d.persp_v2_idle_o) != persp_should_idle) ++persp_idle_mismatches;
    if (persp_should_idle) {
      ++persp_drained_cycles;
    } else if (d.persp_v2_valid_o && !d.persp_out_ready_i) {
      ++persp_held_output_cycles;
    } else {
      ++persp_internal_obligation_cycles;
    }

    const bool rcp_v4_accept = d.rcp_valid_i && d.rcp_v4_ready_o;
    const bool rcp_v4_emit = d.rcp_v4_valid_o && d.rcp_out_ready_i;
    const bool persp_v2_accept = d.persp_valid_i && d.persp_v2_ready_o;
    const bool persp_v2_emit = d.persp_v2_valid_o && d.persp_out_ready_i;

    // Pair advances remain conservative so a differential mismatch cannot make
    // the stimulus streams diverge, but the obligation oracles above/below use
    // successor-only handshakes.
    const bool rcp_accept = d.rcp_valid_i && d.rcp_v3_ready_o && d.rcp_v4_ready_o;
    const bool rcp_emit = d.rcp_v3_valid_o && d.rcp_v4_valid_o && d.rcp_out_ready_i;
    const bool persp_accept = d.persp_valid_i && d.persp_old_ready_o && d.persp_v2_ready_o;
    const bool persp_emit = d.persp_old_valid_o && d.persp_v2_valid_o && d.persp_out_ready_i;

    if (rcp_v4_accept) {
      const uint16_t token = static_cast<uint16_t>(d.rcp_tok_i);
      if ((token >> 8) == 0 || rcp_token_live[token])
        ++rcp_token_roundtrip_errors;
      else
        rcp_token_live[token] = true;
      if ((token >> 8) != 0) ++rcp_high_identity_accepts;
    }
    if (rcp_v4_emit) {
      const uint16_t token = static_cast<uint16_t>(d.rcp_v4_tok_o);
      if ((token >> 8) == 0 || !rcp_token_live[token])
        ++rcp_token_roundtrip_errors;
      else
        rcp_token_live[token] = false;
    }
    if (persp_v2_accept) {
      const uint16_t tag = static_cast<uint16_t>(d.persp_tag_i);
      if ((tag >> 8) == 0 || persp_tag_live[tag])
        ++persp_tag_roundtrip_errors;
      else
        persp_tag_live[tag] = true;
      if ((tag >> 8) != 0) ++persp_high_identity_accepts;
    }
    if (persp_v2_emit) {
      const uint16_t tag = static_cast<uint16_t>(d.persp_v2_tag_o);
      if ((tag >> 8) == 0 || !persp_tag_live[tag])
        ++persp_tag_roundtrip_errors;
      else
        persp_tag_live[tag] = false;
    }

    if (rcp_accept && rcp_denominator(rcp_issued) == 0) ++expected_rcp_zero;
    if (persp_accept && ((persp_issued % 19u) == 0u)) ++expected_persp_zero;

    tick(d);
    rcp_obligations += (rcp_v4_accept ? 1 : 0) - (rcp_v4_emit ? 1 : 0);
    persp_obligations += (persp_v2_accept ? 1 : 0) - (persp_v2_emit ? 1 : 0);
    if (rcp_accept) ++rcp_issued;
    if (rcp_emit) ++rcp_retired;
    if (persp_accept) ++persp_issued;
    if (persp_emit) ++persp_retired;
  }

  d.rcp_valid_i = 0;
  d.rcp_out_ready_i = 1;
  d.persp_valid_i = 0;
  d.persp_out_ready_i = 1;
  for (int i = 0; i < 8; ++i) {
    d.eval();
    if (!same_rcp_cycle(d)) ++rcp_cycle_mismatches;
    if (!same_persp_cycle(d)) ++persp_cycle_mismatches;
    if (rcp_obligations < 0 ||
        d.rcp_v4_occupancy_o != static_cast<uint32_t>(rcp_obligations))
      ++rcp_occupancy_mismatches;
    if (static_cast<bool>(d.rcp_v4_idle_o) != (rcp_obligations == 0))
      ++rcp_idle_mismatches;
    if (persp_obligations < 0 ||
        d.persp_v2_occupancy_o != static_cast<uint32_t>(persp_obligations))
      ++persp_occupancy_mismatches;
    if (static_cast<bool>(d.persp_v2_idle_o) != (persp_obligations == 0))
      ++persp_idle_mismatches;
    tick(d);
  }
  d.eval();

  std::printf(
      "  RCP v3/v4: issued %u retired %u, cycle/occupancy/idle mismatches %u/%u/%u, "
      "independent obligations %d, internal/held/drained cycles %u/%u/%u\n",
      rcp_issued, rcp_retired, rcp_cycle_mismatches, rcp_occupancy_mismatches,
      rcp_idle_mismatches, rcp_obligations, rcp_internal_obligation_cycles,
      rcp_held_output_cycles, rcp_drained_cycles);
  std::printf(
      "  PERSPUV old/v2: issued %u retired %u, cycle/occupancy/idle mismatches %u/%u/%u, "
      "independent obligations %d, internal/held/drained cycles %u/%u/%u\n",
      persp_issued, persp_retired, persp_cycle_mismatches, persp_occupancy_mismatches,
      persp_idle_mismatches, persp_obligations, persp_internal_obligation_cycles,
      persp_held_output_cycles, persp_drained_cycles);

  check(elapsed < 300000 && rcp_issued == kJobs && rcp_retired == kJobs,
        "RCP bounded workload fully accepted and retired", kJobs * 2,
        static_cast<uint64_t>(rcp_issued) + rcp_retired);
  check(rcp_obligations == 0,
        "RCP successor-handshake obligation oracle drains to zero", 0,
        static_cast<uint64_t>(rcp_obligations));
  check(rcp_cycle_mismatches == 0,
        "RCP V4 matches V3 ready/valid/payload/every counter on every cycle", 0,
        rcp_cycle_mismatches);
  const bool rcp_identity_roundtrip =
      rcp_high_identity_accepts == kJobs && rcp_token_roundtrip_errors == 0;
  check(rcp_identity_roundtrip,
        "RCP accepts 512 distinctive nonzero token[13:8] values and roundtrips every "
        "complete 14-bit token exactly once",
        1, rcp_identity_roundtrip ? 1 : 0);
  check(rcp_occupancy_mismatches == 0,
        "RCP occupancy equals successor-handshake accepted-minus-retired every cycle", 0,
        rcp_occupancy_mismatches);
  check(rcp_idle_mismatches == 0,
        "RCP idle is exactly independent accepted-minus-retired equals zero every cycle", 0,
        rcp_idle_mismatches);
  check(rcp_internal_obligation_cycles > 0 && rcp_held_output_cycles > 0 &&
            rcp_drained_cycles > 0,
        "RCP idle control exercised internal, held-output, and drained states", 1,
        (rcp_internal_obligation_cycles > 0 && rcp_held_output_cycles > 0 &&
         rcp_drained_cycles > 0));
  check(d.rcp_v4_accepted_o == kJobs && d.rcp_v4_completed_o == kJobs &&
            d.rcp_v4_zero_jobs_o == expected_rcp_zero &&
            d.rcp_v4_mul_jobs_o == 4u * (kJobs - expected_rcp_zero) &&
            d.rcp_v4_phase_jobs_o == d.rcp_v4_mul_jobs_o + d.rcp_v4_zero_jobs_o &&
            d.rcp_v4_qerr_o == 0,
        "RCP final accounting closes independently", 1,
        (d.rcp_v4_accepted_o == kJobs && d.rcp_v4_completed_o == kJobs &&
         d.rcp_v4_zero_jobs_o == expected_rcp_zero &&
         d.rcp_v4_mul_jobs_o == 4u * (kJobs - expected_rcp_zero) &&
         d.rcp_v4_phase_jobs_o == d.rcp_v4_mul_jobs_o + d.rcp_v4_zero_jobs_o &&
         d.rcp_v4_qerr_o == 0));

  check(elapsed < 300000 && persp_issued == kJobs && persp_retired == kJobs,
        "PERSPUV bounded workload fully accepted and retired", kJobs * 2,
        static_cast<uint64_t>(persp_issued) + persp_retired);
  check(persp_obligations == 0,
        "PERSPUV successor-handshake obligation oracle drains to zero", 0,
        static_cast<uint64_t>(persp_obligations));
  check(persp_cycle_mismatches == 0,
        "PERSPUV V2 matches predecessor ready/valid/payload/every counter every cycle", 0,
        persp_cycle_mismatches);
  const bool persp_identity_roundtrip =
      persp_high_identity_accepts == kJobs && persp_tag_roundtrip_errors == 0;
  check(persp_identity_roundtrip,
        "PERSPUV accepts 512 distinctive nonzero tag[13:8] values and roundtrips every "
        "complete 14-bit tag exactly once",
        1, persp_identity_roundtrip ? 1 : 0);
  check(persp_occupancy_mismatches == 0,
        "PERSPUV occupancy equals successor-handshake accepted-minus-retired every cycle", 0,
        persp_occupancy_mismatches);
  check(persp_idle_mismatches == 0,
        "PERSPUV idle is exactly independent accepted-minus-retired equals zero every cycle", 0,
        persp_idle_mismatches);
  check(persp_internal_obligation_cycles > 0 && persp_held_output_cycles > 0 &&
            persp_drained_cycles > 0,
        "PERSPUV idle control exercised internal, held-output, and drained states", 1,
        (persp_internal_obligation_cycles > 0 && persp_held_output_cycles > 0 &&
         persp_drained_cycles > 0));
  check(d.persp_v2_fragments_o == kJobs &&
            d.persp_v2_zero_products_o == expected_persp_zero &&
            d.persp_v2_products_o == 2u * (kJobs - expected_persp_zero),
        "PERSPUV final accounting closes independently", 1,
        (d.persp_v2_fragments_o == kJobs &&
         d.persp_v2_zero_products_o == expected_persp_zero &&
         d.persp_v2_products_o == 2u * (kJobs - expected_persp_zero)));

  if (g_failures == 0)
    std::printf("[packetb_observation_successors] %d checks passed\n", g_checks);
  else
    std::printf("[packetb_observation_successors] %d/%d checks FAILED\n",
                g_failures, g_checks);
  std::fflush(nullptr);
  std::_Exit(g_failures == 0 ? 0 : 1);
}
