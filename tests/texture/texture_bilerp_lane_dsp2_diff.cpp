// texture_bilerp_lane_dsp2_diff.cpp -- cycle-exact V2/BIL2 differential.
#include "Vtb_texture_bilerp_lane_dsp2.h"

#include <cstdint>
#include <cstdio>
#include <deque>
#include <unordered_set>
#include <vector>

#include "verilated.h"
#include "zhao_sim.hpp"

namespace {

using Dut = Vtb_texture_bilerp_lane_dsp2;

struct Job {
  uint8_t t00, t10, t01, t11, fu, fv;
  uint32_t token;
  uint8_t channel;
};

struct Expected {
  uint8_t value;
  uint32_t token;
  uint8_t channel;
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

void drive(Dut& d, const Job& j) {
  d.t00_i = j.t00;
  d.t10_i = j.t10;
  d.t01_i = j.t01;
  d.t11_i = j.t11;
  d.fu_i = j.fu;
  d.fv_i = j.fv;
  d.tok_i = j.token;
  d.chan_i = j.channel;
}

void reset(Dut& d) {
  d.job_valid_i = 0;
  d.out_ready_i = 0;
  d.rst_n = 0;
  for (int i = 0; i < 5; ++i) zhao::tick(d);
  d.rst_n = 1;
  zhao::tick(d);
  d.eval();
}

bool same_control(const Dut& d) {
  const bool payload_equal = !d.old_out_valid_o ||
      (d.old_out_tok_o == d.new_out_tok_o &&
       d.old_out_chan_o == d.new_out_chan_o);
  return d.old_job_ready_o == d.new_job_ready_o &&
         d.old_out_valid_o == d.new_out_valid_o && payload_equal &&
         d.old_idle_o == d.new_idle_o &&
         d.old_jobs_o == d.new_jobs_o &&
         d.old_occupancy_o == d.new_occupancy_o;
}

void reset_occupancy_controls(Dut& d) {
  for (unsigned depth = 1; depth <= 3; ++depth) {
    reset(d);
    d.out_ready_i = 0;
    for (unsigned i = 0; i < depth; ++i) {
      Job j{static_cast<uint8_t>(i), 255, static_cast<uint8_t>(255 - i), 0,
            200, 91, 0x100u + i, static_cast<uint8_t>(i)};
      d.job_valid_i = 1;
      drive(d, j);
      d.eval();
      zhao::check(same_control(d), "BIL2 reset setup keeps cycle control equal", 1,
                  same_control(d) ? 1 : 0);
      zhao::tick(d);
    }
    d.eval();
    zhao::check(d.old_occupancy_o == depth && d.new_occupancy_o == depth,
                "BIL2 reset reaches each occupancy", depth,
                (d.old_occupancy_o == depth && d.new_occupancy_o == depth) ? depth : 0);
    d.job_valid_i = 0;
    d.rst_n = 0;
    zhao::tick(d);
    d.rst_n = 1;
    zhao::tick(d);
    d.eval();
    zhao::check(d.old_idle_o && d.new_idle_o && d.old_jobs_o == 0 &&
                    d.new_jobs_o == 0 && d.old_occupancy_o == 0 &&
                    d.new_occupancy_o == 0,
                "BIL2 reset clears every live stage and counter", 1,
                (d.old_idle_o && d.new_idle_o && d.old_jobs_o == 0 &&
                 d.new_jobs_o == 0 && d.old_occupancy_o == 0 &&
                 d.new_occupancy_o == 0) ? 1 : 0);
  }
}

void run_stream(Dut& d) {
  std::vector<Job> jobs;
  auto add = [&](unsigned t00, unsigned t10, unsigned t01, unsigned t11,
                 unsigned fu, unsigned fv) {
    const uint32_t n = static_cast<uint32_t>(jobs.size());
    jobs.push_back(Job{static_cast<uint8_t>(t00), static_cast<uint8_t>(t10),
                       static_cast<uint8_t>(t01), static_cast<uint8_t>(t11),
                       static_cast<uint8_t>(fu), static_cast<uint8_t>(fv),
                       (n * 7919u + 0x155u) & 0x3ffffu,
                       static_cast<uint8_t>(n & 3u)});
  };

  // Unlike horizontal products plus byte/fraction rails and ties.
  add(0, 255, 255, 0, 191, 173);
  add(255, 0, 0, 255, 255, 128);
  add(0, 255, 0, 255, 128, 0);
  add(0, 255, 255, 0, 128, 128);
  const unsigned rails[] = {0, 1, 127, 128, 254, 255};
  for (unsigned t00 : rails)
    for (unsigned t10 : rails)
      for (unsigned t01 : rails)
        for (unsigned t11 : rails)
          for (unsigned f : rails)
            add(t00, t10, t01, t11, f, rails[(f + t01) % 6]);

  uint32_t seed = 0xb112d5f2u;
  std::unordered_set<uint64_t> random_tuples;
  for (int i = 0; i < 4000; ++i) {
    const uint32_t texels = next_random(&seed);
    const uint32_t fractions = next_random(&seed);
    add(texels, texels >> 8, texels >> 16, texels >> 24,
        fractions, fractions >> 8);
    random_tuples.insert((static_cast<uint64_t>(texels) << 16) |
                         (fractions & 0xffffu));
  }
  zhao::check(random_tuples.size() == 4000,
              "BIL2 random sweep contains 4000 distinct six-byte tuples", 4000,
              random_tuples.size());

  reset(d);
  std::deque<Expected> expected;
  size_t offered = 0;
  size_t retired = 0;
  int cycle = 0;
  int control_mismatch = 0;
  int old_oracle_mismatch = 0;
  int new_oracle_mismatch = 0;
  int old_hold_mismatch = 0;
  int new_hold_mismatch = 0;
  int value_difference = 0;
  int max_occupancy = 0;
  int hold_checks = 0;
  int first_accept = -1;
  int first_retire = -1;
  bool held_old = false;
  bool held_new = false;
  Expected old_packet{};
  Expected new_packet{};

  while ((offered < jobs.size() || retired < jobs.size()) && cycle < 200000) {
    d.out_ready_i = (cycle < 96 || ((cycle % 17) != 4 &&
                                    (cycle % 17) != 5 &&
                                    (cycle % 17) != 6)) ? 1 : 0;
    d.job_valid_i = offered < jobs.size();
    if (offered < jobs.size()) drive(d, jobs[offered]);
    d.eval();

    if (!same_control(d)) ++control_mismatch;
    if (d.old_occupancy_o > max_occupancy) max_occupancy = d.old_occupancy_o;

    if (held_old) {
      ++hold_checks;
      if (!d.old_out_valid_o || d.old_out_o != old_packet.value ||
          d.old_out_tok_o != old_packet.token ||
          d.old_out_chan_o != old_packet.channel)
        ++old_hold_mismatch;
    }
    if (held_new && (!d.new_out_valid_o || d.new_out_o != new_packet.value ||
                     d.new_out_tok_o != new_packet.token ||
                     d.new_out_chan_o != new_packet.channel))
      ++new_hold_mismatch;

    if (d.old_out_valid_o && d.out_ready_i) {
      if (first_retire < 0) first_retire = cycle;
      if (expected.empty()) {
        ++old_oracle_mismatch;
      } else {
        const Expected want = expected.front();
        if (d.old_out_o != want.value || d.old_out_tok_o != want.token ||
            d.old_out_chan_o != want.channel)
          ++old_oracle_mismatch;
        if (d.new_out_o != want.value || d.new_out_tok_o != want.token ||
            d.new_out_chan_o != want.channel)
          ++new_oracle_mismatch;
        if (d.old_out_o != d.new_out_o) ++value_difference;
        expected.pop_front();
      }
      ++retired;
    }

    const bool accept = d.job_valid_i && d.old_job_ready_o;
    if (accept) {
      if (first_accept < 0) first_accept = cycle;
      const Job& j = jobs[offered++];
      expected.push_back(Expected{oracle(j), j.token, j.channel});
    }

    held_old = d.old_out_valid_o && !d.out_ready_i;
    held_new = d.new_out_valid_o && !d.out_ready_i;
    if (held_old)
      old_packet = Expected{static_cast<uint8_t>(d.old_out_o),
                            static_cast<uint32_t>(d.old_out_tok_o),
                            static_cast<uint8_t>(d.old_out_chan_o)};
    if (held_new)
      new_packet = Expected{static_cast<uint8_t>(d.new_out_o),
                            static_cast<uint32_t>(d.new_out_tok_o),
                            static_cast<uint8_t>(d.new_out_chan_o)};

    zhao::tick(d);
    ++cycle;
  }

  d.job_valid_i = 0;
  d.out_ready_i = 1;
  d.eval();
  zhao::check(offered == jobs.size(), "BIL2 accepts every offered job", jobs.size(), offered);
  zhao::check(retired == jobs.size(), "BIL2 retires every job once", jobs.size(), retired);
  zhao::check(control_mismatch == 0, "BIL2 cycle control/token/channel parity", 0,
              control_mismatch);
  zhao::check(old_oracle_mismatch == 0, "old bilerp remains independent oracle", 0,
              old_oracle_mismatch);
  zhao::check(old_hold_mismatch == 0 && new_hold_mismatch == 0,
              "both bilerp implementations hold complete output packets", 0,
              old_hold_mismatch + new_hold_mismatch);
  zhao::check(first_retire - first_accept == 3, "BIL2 latency remains three clocks", 3,
              first_retire - first_accept);
  zhao::check(max_occupancy == 3, "BIL2 reaches full three-stage occupancy", 3,
              max_occupancy);
  zhao::check(hold_checks > 0, "BIL2 output hold is exercised", 1,
              hold_checks > 0 ? 1 : 0);
  zhao::check(d.old_idle_o && d.new_idle_o, "both bilerp implementations drain", 1,
              (d.old_idle_o && d.new_idle_o) ? 1 : 0);

#ifdef EXPECT_BIL2_COLLAPSE_RESULTB
  zhao::check(value_difference > 0 && new_oracle_mismatch > 0,
              "BIL2 collapse-resultb mutant is detected", 1,
              (value_difference > 0 && new_oracle_mismatch > 0) ? 1 : 0);
  std::printf("BIL2 collapse-resultb mutant FIRED differences=%d\n", value_difference);
#else
  zhao::check(new_oracle_mismatch == 0 && value_difference == 0,
              "BIL2 candidate matches old and host oracle exactly", 0,
              new_oracle_mismatch + value_difference);
#endif
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;
  reset_occupancy_controls(dut);
  run_stream(dut);
  return zhao::report_and_exit("texture_bilerp_lane_dsp2_diff");
}
