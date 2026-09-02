// texture_bilerp_lane_directed.cpp — is the pipelined filter bit-identical to
// the shipped one, and does it really take a job every clock?
//
// ---------------------------------------------------------------------------
// TWO CLAIMS
// ---------------------------------------------------------------------------
// 1. BIT-IDENTICAL. The lane splits `zhao_texture_bilerp`'s expressions at a
//    register between two EXACT intermediates, so nothing can round
//    differently. That is a construction argument, and construction arguments
//    are how wrong arithmetic gets shipped -- so the shipped combinational
//    block runs on the same stimulus and every byte is compared.
//
// 2. II=1. The brief asks for "one channel job per clock". A three-stage
//    filter that accepted a job every THIRD clock would be bit-identical and
//    useless, so occupancy is asserted directly.
//
// The stimulus includes the corners the filter's rounding lives at: both
// fractions 0 and 255, all four texels equal, and the maximum gradient. A
// random sweep alone would very likely miss fu=fv=0, which is the case where
// the answer must be exactly t00 and any bias shows up immediately.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"

#include "Vtb_bilerp_pair.h"

#include "zhao_sim.hpp"

namespace {

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

struct Job {
  uint8_t t00, t10, t01, t11, fu, fv;
  uint16_t tok;
  uint8_t chan;
  uint8_t expect;   // filled from the combinational reference
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_bilerp_pair top;

  top.lane_valid_i = 0;
  top.lane_ready_i = 1;
  top.rst_n = 0;
  for (int i = 0; i < 6; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  // ---- stimulus ------------------------------------------------------------
  std::vector<Job> jobs;
  auto add = [&](int a, int b, int c, int d, int fu, int fv) {
    Job j{};
    j.t00 = static_cast<uint8_t>(a); j.t10 = static_cast<uint8_t>(b);
    j.t01 = static_cast<uint8_t>(c); j.t11 = static_cast<uint8_t>(d);
    j.fu = static_cast<uint8_t>(fu); j.fv = static_cast<uint8_t>(fv);
    j.tok = static_cast<uint16_t>(jobs.size() + 1);
    j.chan = static_cast<uint8_t>(jobs.size() & 3);
    jobs.push_back(j);
  };

  // corners the rounding lives at
  add(0, 0, 0, 0, 0, 0);
  add(255, 255, 255, 255, 255, 255);
  add(0, 255, 0, 255, 0, 0);      // fu=0 -> must be exactly t00
  add(0, 255, 0, 255, 255, 0);    // fu=255 -> nearly t10
  add(0, 0, 255, 255, 0, 255);    // fv=255 -> nearly t01
  add(0, 255, 255, 0, 128, 128);  // the diagonal, half and half
  add(17, 200, 33, 240, 1, 254);
  add(200, 17, 240, 33, 254, 1);

  uint32_t s = 0x0BADF00Du;
  for (int i = 0; i < 400; ++i)
    add(rnd(&s) & 255, rnd(&s) & 255, rnd(&s) & 255, rnd(&s) & 255, rnd(&s) & 255,
        rnd(&s) & 255);

  // ---- capture the combinational reference for each job -------------------
  // Purely combinational, so drive and read in the same evaluation.
  for (Job& j : jobs) {
    top.t00_i = j.t00; top.t10_i = j.t10;
    top.t01_i = j.t01; top.t11_i = j.t11;
    top.fu_i = j.fu;   top.fv_i = j.fv;
    top.eval();
    j.expect = top.ref_o;
  }

  // ---- run the lane, one job per clock ------------------------------------
  std::deque<Job> inflight;
  size_t fed = 0, got = 0;
  int bad_val = 0, bad_tok = 0, bad_chan = 0, max_occ = 0;

  for (int c = 0; c < static_cast<int>(jobs.size()) + 64 && got < jobs.size(); ++c) {
    const bool feeding = fed < jobs.size();
    top.lane_valid_i = feeding;
    if (feeding) {
      const Job& j = jobs[fed];
      top.t00_i = j.t00; top.t10_i = j.t10;
      top.t01_i = j.t01; top.t11_i = j.t11;
      top.fu_i = j.fu;   top.fv_i = j.fv;
      top.lane_tok_i = j.tok;
      top.lane_chan_i = j.chan;
    }
    top.eval();

    if (top.lane_valid_o && top.lane_ready_i) {
      if (inflight.empty()) {
        ++bad_val;
      } else {
        const Job e = inflight.front();
        inflight.pop_front();
        if (top.lane_out_o != e.expect) ++bad_val;
        if (top.lane_tok_o != e.tok) ++bad_tok;
        if (top.lane_chan_o != e.chan) ++bad_chan;
        ++got;
      }
    }
    if (top.lane_occ_o > max_occ) max_occ = top.lane_occ_o;

    const bool took = feeding && top.lane_ready_o;
    if (took) {
      inflight.push_back(jobs[fed]);
      ++fed;
    }
    zhao::tick(top);
  }
  top.lane_valid_i = 0;

  zhao::check(fed == jobs.size(), "every job was offered", jobs.size(), fed);
  zhao::check(got == jobs.size(), "and every job came back exactly once", jobs.size(),
              got);
  zhao::check(bad_val == 0,
              "every filtered byte is BIT-IDENTICAL to the shipped bilerp", 0,
              bad_val);
  zhao::check(bad_tok == 0, "and carries its own job's token", 0, bad_tok);
  zhao::check(bad_chan == 0, "and its own channel", 0, bad_chan);

  // II=1: three stages must all be occupied under a steady feed. A filter that
  // took a job every third clock would pass every check above.
  zhao::check(max_occ == 3, "three jobs are in flight at once (II=1, not 1-in-3)",
              3, max_occ);

  std::printf("  %zu jobs, max occupancy %d, %u accepted\n", jobs.size(), max_occ,
              top.lane_jobs_o);

  return zhao::report_and_exit("texture_bilerp_lane_directed");
}
