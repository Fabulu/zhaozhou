// raster_rcp24_v4_timing_directed.cpp -- the V4 tile against the frozen
// reciprocal oracle, while its acceptance, retirement, and hold timing is
// observed cycle by cycle.
//
// V4 is deliberately driven directly rather than through a comparison wrapper.
// The acceptance-captured exponent and zero bit are part of the timing contract;
// a wrapper that re-derives either value after the request has moved would not
// test that contract.

#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

#include "verilated.h"

#include "Vzhao_raster_rcp24_v4.h"

#include "zhao_sim.hpp"
#include "zref/zref_rcp.hpp"

namespace {

using Top = Vzhao_raster_rcp24_v4;

struct Request {
  uint32_t d;
  uint32_t tok;
};

struct Expected {
  uint32_t r;
  uint32_t k;
  bool zero;
};

struct Output {
  uint32_t r;
  uint32_t k;
  bool zero;
  uint32_t tok;

  bool operator!=(const Output& other) const {
    return r != other.r || k != other.k || zero != other.zero || tok != other.tok;
  }
};

// rcp_u24 has no zero result -- zero is the separately specified terminal
// phase.  Nonzero values, including d == 1 and every boundary below, use the
// committed reference implementation rather than a second C++ derivation.
Expected oracle(uint32_t d) {
  if (d == 0) return Expected{0, 0, true};
  const zref::rcp24_result ref = zref::rcp_u24(d);
  return Expected{ref.r, static_cast<uint32_t>(ref.k), false};
}

// Read only the high half of the state.  The low bits of a power-of-two LCG
// would make a superficially random test repeat a tiny pattern.
struct Lcg {
  uint64_t state;

  explicit Lcg(uint64_t seed) : state(seed) {}

  uint32_t u32() {
    state = state * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<uint32_t>(state >> 32);
  }
};

std::vector<Request> make_stream() {
  const uint32_t kMax = 0xFFFFFFu;
  std::vector<uint32_t> special = {
      0u,       1u,       2u,       3u,       0x7FFFFEu, 0x7FFFFFu,
      0x800000u, 0x800001u, 0xFFFFFEu, 0xFFFFFFu, 0xABCDEFu,
  };

  // Every exponent is represented explicitly.  The neighbours straddle every
  // leading-bit decision that the five-level reducer makes.
  for (int bit = 0; bit < 24; ++bit) special.push_back(1u << bit);
  for (int bit = 1; bit <= 23; ++bit) {
    const uint32_t boundary = 1u << bit;
    special.push_back(boundary - 1u);
    special.push_back(boundary);
    if (boundary < kMax) special.push_back(boundary + 1u);
  }

  std::vector<uint32_t> random;
  random.reserve(137);
  Lcg rng(0xD1CE5EED5A17C0DEull);
  for (int i = 0; i < 137; ++i) {
    uint32_t d = rng.u32() & kMax;
    if ((i % 31) == 0) d = 0;  // keep the scheduled zero phase in the random tail
    random.push_back(d);
  }

  // Alternate the directed corpus and the random tail.  This matters: a test
  // with all powers first and all random values later can hide an acceptance
  // field that is sampled from the wrong cycle.
  std::vector<uint32_t> values;
  values.reserve(special.size() + random.size());
  size_t si = 0;
  size_t ri = 0;
  while (si < special.size() || ri < random.size()) {
    if (si < special.size()) values.push_back(special[si++]);
    if (ri < random.size()) values.push_back(random[ri++]);
    if (si < special.size()) values.push_back(special[si++]);
  }

  std::vector<Request> stream;
  stream.reserve(values.size());
  for (size_t i = 0; i < values.size(); ++i)
    stream.push_back(Request{values[i], static_cast<uint32_t>(i)});
  return stream;
}

void reset(Top& top) {
  top.rst_n = 0;
  top.v_valid_i = 0;
  top.d_i = 0;
  top.v_tok_i = 0;
  top.r_ready_i = 1;
  top.eval();
  for (int i = 0; i < 6; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();
}

bool output_ready(Lcg& rng, uint64_t cycle) {
  // The two-cycle holes guarantee real output holds once the pipeline is full;
  // the forced-ready points prevent a random draw from becoming a liveness
  // assumption.
  const bool scheduled_gap = (cycle % 11u == 4u) || (cycle % 11u == 5u);
  const bool random_ready = (rng.u32() & 0x80000000u) != 0;
  return !scheduled_gap && ((cycle % 7u) == 0u || random_ready);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Top top;
  reset(top);

  const std::vector<Request> stream = make_stream();
  zhao::check(stream.size() < 256,
              "the token scoreboard has no live-token wraparound", 1,
              stream.size() < 256 ? 1 : 0);

  uint32_t expected_zero_jobs = 0;
  for (const Request& request : stream)
    if (request.d == 0) ++expected_zero_jobs;
  const uint32_t expected_mul_jobs =
      4u * (static_cast<uint32_t>(stream.size()) - expected_zero_jobs);
  const uint32_t expected_phase_jobs = expected_mul_jobs + expected_zero_jobs;

  std::map<uint32_t, Expected> pending;
  size_t sent = 0;
  uint32_t accepted = 0;
  uint32_t retired = 0;
  uint64_t cycles = 0;
  uint32_t peak_occupancy = 0;
  uint32_t hold_cycles = 0;
  uint32_t input_stall_cycles = 0;
  uint32_t counter_sync_errors = 0;
  uint32_t occupancy_errors = 0;
  uint32_t idle_errors = 0;
  uint32_t input_hold_errors = 0;
  uint32_t output_hold_errors = 0;
  uint32_t token_errors = 0;
  uint32_t qerr_observations = 0;
  uint32_t arithmetic_mismatches = 0;
  uint32_t k_mismatches = 0;
  uint32_t zero_mismatches = 0;
  uint32_t first_bad_d = 0;
  uint32_t first_bad_tok = 0;
  bool have_first_bad = false;

  bool input_held = false;
  Request held_input{0, 0};
  bool output_held = false;
  Output held_output{0, 0, false, 0};
  Lcg ready_rng(0xA11CE5EED1234567ull);

  // The bound is deliberately generous compared with the ten-stage feedback
  // path.  A failure to drain is a timing/handshake failure, not a test skip.
  for (; cycles < 200000 && (sent < stream.size() || retired < stream.size()); ++cycles) {
    const bool offering = sent < stream.size();
    if (offering) {
      top.v_valid_i = 1;
      top.d_i = stream[sent].d;
      top.v_tok_i = static_cast<uint8_t>(stream[sent].tok);
    } else {
      top.v_valid_i = 0;
      top.d_i = 0;
      top.v_tok_i = 0;
    }
    top.r_ready_i = output_ready(ready_rng, cycles) ? 1 : 0;
    top.eval();

    const uint32_t hw_accepted = static_cast<uint32_t>(top.accepted_o);
    const uint32_t hw_completed = static_cast<uint32_t>(top.completed_o);
    const uint32_t hw_occupancy = static_cast<uint32_t>(top.occupancy_o);
    const uint32_t counter_delta = hw_accepted - hw_completed;
    if (hw_accepted != accepted || hw_completed != retired) ++counter_sync_errors;
    if (hw_occupancy != counter_delta) ++occupancy_errors;
    if ((top.idle_o != 0) != (hw_occupancy == 0)) ++idle_errors;
    if (top.qerr_o != 0) ++qerr_observations;
    if (hw_occupancy > peak_occupancy) peak_occupancy = hw_occupancy;

    if (offering && top.v_ready_o == 0) ++input_stall_cycles;
    if (input_held &&
        (!offering || static_cast<uint32_t>(top.d_i) != held_input.d ||
         static_cast<uint32_t>(top.v_tok_i) != held_input.tok)) {
      ++input_hold_errors;
    }

    const bool result_valid = top.r_valid_o != 0;
    const bool result_ready = top.r_ready_i != 0;
    const bool result_fire = result_valid && result_ready;
    const Output now{static_cast<uint32_t>(top.r_o), static_cast<uint32_t>(top.k_o),
                     top.d_zero_o != 0, static_cast<uint32_t>(top.r_tok_o)};

    // A result is checked while it is held, not only after it has disappeared.
    // That is the observation that catches a done-head changing under
    // backpressure.
    if (output_held && (!result_valid || now != held_output)) ++output_hold_errors;
    if (result_valid && !result_ready) {
      ++hold_cycles;
      if (!output_held) {
        held_output = now;
        output_held = true;
      }
    }

    if (result_fire) {
      auto it = pending.find(now.tok);
      if (it == pending.end()) {
        ++token_errors;
      } else {
        const Expected& want = it->second;
        const bool arithmetic_bad = now.r != want.r;
        const bool k_bad = now.k != want.k;
        const bool zero_bad = now.zero != want.zero;
        if (arithmetic_bad) ++arithmetic_mismatches;
        if (k_bad) ++k_mismatches;
        if (zero_bad) ++zero_mismatches;
        if ((arithmetic_bad || k_bad || zero_bad) && !have_first_bad) {
          have_first_bad = true;
          first_bad_d = 0;
          for (const Request& request : stream) {
            if (request.tok == now.tok) {
              first_bad_d = request.d;
              break;
            }
          }
          first_bad_tok = now.tok;
        }
        pending.erase(it);
      }
      ++retired;
      output_held = false;
    }

    const bool input_fire = offering && top.v_ready_o != 0;
    if (input_fire) {
      const Request& request = stream[sent];
      if (!pending.emplace(request.tok, oracle(request.d)).second) ++token_errors;
      ++sent;
      ++accepted;
    }
    if (offering && !input_fire) {
      input_held = true;
      held_input = stream[sent];
    } else {
      input_held = false;
    }

    zhao::tick(top);
  }

  top.v_valid_i = 0;
  top.d_i = 0;
  top.v_tok_i = 0;
  top.r_ready_i = 1;
  top.eval();

  // Check the settled state once more after the final retirement edge.  The
  // per-cycle checks above intentionally use the same pre-edge convention as
  // ready/valid handshakes.
  const uint32_t final_accepted = static_cast<uint32_t>(top.accepted_o);
  const uint32_t final_completed = static_cast<uint32_t>(top.completed_o);
  const uint32_t final_occupancy = static_cast<uint32_t>(top.occupancy_o);
  if (final_accepted != accepted || final_completed != retired) ++counter_sync_errors;
  if (final_occupancy != final_accepted - final_completed) ++occupancy_errors;
  if ((top.idle_o != 0) != (final_occupancy == 0)) ++idle_errors;
  if (top.qerr_o != 0) ++qerr_observations;

  std::printf("  stream: %zu accepted, %u retired in %llu clocks; peak occupancy %u\n",
              stream.size(), retired, static_cast<unsigned long long>(cycles), peak_occupancy);
  std::printf("  holds: %u output-hold cycles, %u input-stall cycles\n", hold_cycles,
              input_stall_cycles);
  std::printf("  value mismatches: arithmetic=%u k=%u zero=%u\n", arithmetic_mismatches,
              k_mismatches, zero_mismatches);
  if (have_first_bad)
    std::printf("  first value mismatch: d=0x%06X token=%u\n", first_bad_d, first_bad_tok);

  zhao::check(sent == stream.size(), "every request was accepted", stream.size(), sent);
  zhao::check(retired == stream.size(), "every accepted request retired", stream.size(), retired);
  zhao::check(pending.empty(), "the token scoreboard is empty after drain", 0, pending.size());
  zhao::check(counter_sync_errors == 0, "accepted/completed counters track the score", 0,
              counter_sync_errors);
  zhao::check(occupancy_errors == 0,
              "occupancy is accepted minus completed on every observed edge", 0,
              occupancy_errors);
  zhao::check(idle_errors == 0, "idle is exactly the zero-occupancy observation", 0, idle_errors);
  zhao::check(input_hold_errors == 0, "input remains stable under input backpressure", 0,
              input_hold_errors);
  zhao::check(output_hold_errors == 0, "result remains stable under output backpressure", 0,
              output_hold_errors);
  zhao::check(token_errors == 0, "every retired result has exactly one live token", 0,
              token_errors);
  zhao::check(qerr_observations == 0, "no queue overflow or underflow was observed", 0,
              qerr_observations);
  zhao::check(hold_cycles > 0, "output backpressure actually exercised a result hold", 1,
              hold_cycles > 0 ? 1 : 0);
  zhao::check(input_stall_cycles > 0, "input backpressure actually exercised the context bound", 1,
              input_stall_cycles > 0 ? 1 : 0);
  zhao::check(peak_occupancy > 1, "the interleaved stream had multiple live contexts", 1,
              peak_occupancy > 1 ? 1 : 0);

  zhao::check(final_accepted == stream.size(), "accepted_o is exact", stream.size(),
              final_accepted);
  zhao::check(final_completed == stream.size(), "completed_o is exact", stream.size(),
              final_completed);
  zhao::check(static_cast<uint32_t>(top.mul_jobs_o) == expected_mul_jobs,
              "mul_jobs_o is exactly four per nonzero request", expected_mul_jobs,
              top.mul_jobs_o);
  zhao::check(static_cast<uint32_t>(top.zero_jobs_o) == expected_zero_jobs,
              "zero_jobs_o is exactly one per zero request", expected_zero_jobs,
              top.zero_jobs_o);
  zhao::check(static_cast<uint32_t>(top.phase_jobs_o) == expected_phase_jobs,
              "phase_jobs_o is products plus scheduled zero phases", expected_phase_jobs,
              top.phase_jobs_o);
  zhao::check(static_cast<uint32_t>(top.negcorr_jobs_o) == 0,
              "negcorr_jobs_o stays zero on the u24 reciprocal domain", 0,
              top.negcorr_jobs_o);
  zhao::check(final_occupancy == 0, "the final accepted-minus-completed occupancy is zero", 0,
              final_occupancy);
  zhao::check(top.idle_o != 0, "idle_o is asserted after the final drain", 1,
              top.idle_o != 0 ? 1 : 0);

  const uint32_t value_mismatches = arithmetic_mismatches + k_mismatches + zero_mismatches;
#ifdef ZHAO_RCP_V4_EXPECT_LZC_MUTANT
  // This branch is used only with the committed reverse-bit shim.  It passes
  // when the real V4 path exposes the mutant through an arithmetic, exponent,
  // or zero-field mismatch; a silent mutant is a failed positive control.
  zhao::check(value_mismatches != 0,
              "the committed LZC orientation mutant is detected by the oracle", 1,
              value_mismatches != 0 ? 1 : 0);
  if (value_mismatches != 0) std::printf("DETECTED\n");
#else
  zhao::check(value_mismatches == 0,
              "every V4 result matches zref::rcp_u24 in r/k/zero", 0, value_mismatches);
#endif

  return zhao::report_and_exit("raster_rcp24_v4_timing_directed");
}
