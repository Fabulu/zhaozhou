// texture_bilerp_lane_v2_directed.cpp — Packet-B bilerp successor gate.
//
// Checks the exact single-rounding integer law, token/channel carriage, II=1,
// output hold, occupancy, and the new complete idle_o observation.
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"
#include "Vzhao_texture_bilerp_lane_v2.h"
#include "zhao_sim.hpp"

namespace {

struct Job {
  uint8_t t00 = 0;
  uint8_t t10 = 0;
  uint8_t t01 = 0;
  uint8_t t11 = 0;
  uint8_t fu = 0;
  uint8_t fv = 0;
  uint32_t token = 0;
  uint8_t channel = 0;
};

struct Result {
  uint8_t value = 0;
  uint32_t token = 0;
  uint8_t channel = 0;
};

uint32_t next_random(uint32_t* state) {
  *state = *state * 1664525u + 1013904223u;
  return *state;
}

uint8_t oracle(const Job& j) {
  const int64_t a = static_cast<int64_t>(j.t00) * 256 +
                    (static_cast<int64_t>(j.t10) - j.t00) * j.fu;
  const int64_t b = static_cast<int64_t>(j.t01) * 256 +
                    (static_cast<int64_t>(j.t11) - j.t01) * j.fu;
  const int64_t sum = a * 256 + (b - a) * j.fv;
  return static_cast<uint8_t>((sum + 32768) >> 16);
}

void drive(Vzhao_texture_bilerp_lane_v2& top, const Job& j) {
  top.t00_i = j.t00;
  top.t10_i = j.t10;
  top.t01_i = j.t01;
  top.t11_i = j.t11;
  top.fu_i = j.fu;
  top.fv_i = j.fv;
  top.tok_i = j.token;
  top.chan_i = j.channel;
}

void reset(Vzhao_texture_bilerp_lane_v2& top) {
  top.job_valid_i = 0;
  top.out_ready_i = 0;
  top.rst_n = 0;
  for (int i = 0; i < 6; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);
}

void test_lane(Vzhao_texture_bilerp_lane_v2& top) {
  reset(top);
  top.eval();
  zhao::check(top.idle_o == 1, "bilerp-v2 idle after reset", 1, top.idle_o);
  zhao::check(top.occupancy_o == 0, "bilerp-v2 occupancy zero after reset", 0,
              top.occupancy_o);

  std::vector<Job> jobs;
  auto add = [&](int t00, int t10, int t01, int t11, int fu, int fv) {
    Job j;
    j.t00 = static_cast<uint8_t>(t00);
    j.t10 = static_cast<uint8_t>(t10);
    j.t01 = static_cast<uint8_t>(t01);
    j.t11 = static_cast<uint8_t>(t11);
    j.fu = static_cast<uint8_t>(fu);
    j.fv = static_cast<uint8_t>(fv);
    j.token = static_cast<uint32_t>((jobs.size() * 7919u + 3u) & 0x3FFFFu);
    j.channel = static_cast<uint8_t>(jobs.size() & 3u);
    jobs.push_back(j);
  };

  add(0, 0, 0, 0, 0, 0);
  add(255, 255, 255, 255, 255, 255);
  add(0, 255, 0, 255, 0, 0);
  add(0, 255, 0, 255, 128, 0);     // exact half-up tie -> 128
  add(0, 255, 255, 0, 128, 128);
  add(255, 0, 0, 255, 1, 254);
  add(255, 0, 0, 255, 254, 1);
  add(17, 200, 33, 240, 255, 255);

  uint32_t seed = 0xB11E4F17u;
  for (int i = 0; i < 1000; ++i) {
    add(next_random(&seed), next_random(&seed), next_random(&seed), next_random(&seed),
        next_random(&seed), next_random(&seed));
  }

  std::deque<Result> expected;
  size_t offered = 0;
  size_t retired = 0;
  int cycle = 0;
  int wrong = 0;
  int max_occupancy = 0;
  int hold_checks = 0;
  int first_accept = -1;
  int first_retire = -1;
  bool previous_stall = false;
  Result previous;

  while ((offered < jobs.size() || retired < jobs.size()) && cycle < 20000) {
    // First 64 clocks are all-ready to fill all three stages at II=1.  Later
    // stalls exercise backward pressure and immutable held output.
    const bool ready = cycle < 64 ? true : ((cycle % 13) != 5 && (cycle % 13) != 6 &&
                                            (cycle % 13) != 7);
    top.out_ready_i = ready ? 1 : 0;
    top.job_valid_i = offered < jobs.size() ? 1 : 0;
    if (offered < jobs.size()) drive(top, jobs[offered]);
    top.eval();

    if (top.occupancy_o > max_occupancy) max_occupancy = top.occupancy_o;

    if (previous_stall) {
      ++hold_checks;
      if (!top.out_valid_o || top.out_o != previous.value || top.out_tok_o != previous.token ||
          top.out_chan_o != previous.channel)
        ++wrong;
    }

    if (top.out_valid_o && top.out_ready_i) {
      if (first_retire < 0) first_retire = cycle;
      if (expected.empty()) {
        ++wrong;
      } else {
        const Result& want = expected.front();
        if (top.out_o != want.value || top.out_tok_o != want.token ||
            top.out_chan_o != want.channel)
          ++wrong;
        expected.pop_front();
      }
      ++retired;
    }

    const bool accept = top.job_valid_i && top.job_ready_o;
    if (accept) {
      if (first_accept < 0) first_accept = cycle;
      const Job& j = jobs[offered];
      expected.push_back(Result{oracle(j), j.token, j.channel});
      ++offered;
    }

    previous_stall = top.out_valid_o && !top.out_ready_i;
    if (previous_stall)
      previous = Result{static_cast<uint8_t>(top.out_o), static_cast<uint32_t>(top.out_tok_o),
                        static_cast<uint8_t>(top.out_chan_o)};

    zhao::tick(top);
    ++cycle;
  }

  top.job_valid_i = 0;
  top.out_ready_i = 1;
  top.eval();
  zhao::check(offered == jobs.size(), "bilerp-v2 accepts every job", jobs.size(), offered);
  zhao::check(retired == jobs.size(), "bilerp-v2 returns every job once", jobs.size(), retired);
  zhao::check(wrong == 0, "bilerp-v2 value/token/channel and hold are exact", 0, wrong);
  zhao::check(first_retire - first_accept == 3, "bilerp-v2 latency remains three clocks", 3,
              first_retire - first_accept);
  zhao::check(max_occupancy == 3, "bilerp-v2 fills all three stages at II=1", 3,
              max_occupancy);
  zhao::check(hold_checks > 0, "bilerp-v2 output stall exercised hold", 1,
              hold_checks > 0 ? 1 : 0);
  zhao::check(top.jobs_o == jobs.size(), "bilerp-v2 counts accepted jobs", jobs.size(),
              top.jobs_o);
  zhao::check(top.idle_o == 1, "bilerp-v2 returns to complete idle", 1, top.idle_o);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_bilerp_lane_v2 top;
  test_lane(top);
  return zhao::report_and_exit("texture_bilerp_lane_v2_directed");
}
