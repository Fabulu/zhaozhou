// texture_aux_div6_directed.cpp — is the pipelined AUX divide exact, and is it
// actually II=1 with six requests in flight?
//
// ---------------------------------------------------------------------------
// THE ORACLE IS INDEPENDENT
// ---------------------------------------------------------------------------
// The quotient is checked against `n / d` computed in C++, NOT against a
// re-implementation of the RTL's restoring steps. A test that repeats the
// design's own arithmetic proves the two copies agree and nothing else -- it
// would pass just as happily if both were wrong, which is the failure this
// project has already had in other forms.
//
// The pipeline's contract is the same one `zhao_texture_aux.sv` documents: the
// caller has already handled N < 0 (answer 0) and N >= 64*D (answer 63), so
// inside the divide the quotient is in [0,63] and six restoring steps suffice.
// Every stimulus here respects that precondition, and the test says so rather
// than quietly generating only easy numbers.
//
// ---------------------------------------------------------------------------
// AND II=1 IS MEASURED, NOT ASSUMED
// ---------------------------------------------------------------------------
// The whole point of the rebuild is six requests walking through together
// instead of one occupying the divider for two clocks. A pipeline that produced
// correct answers one-at-a-time would pass a correctness test perfectly and
// deliver none of the throughput, so occupancy is asserted directly: feed one
// request per clock and require the block to report SIX in flight.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"

#include "Vzhao_texture_aux_div6.h"

#include "zhao_sim.hpp"

namespace {

constexpr int kLatency = 6;

struct Job {
  uint64_t nu, du, nv, dv;
  uint8_t tag;
  uint32_t qu, qv;  // the independent oracle
};

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_aux_div6 top;

  top.in_valid_i = 0;
  top.rst_n = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  // ---- build a batch that respects the caller's precondition --------------
  uint32_t s = 0xC0FFEEu;
  std::vector<Job> jobs;
  for (int i = 0; i < 512; ++i) {
    Job j{};
    // Denominators from 1 upward; a zero denominator is the caller's degenerate
    // case and is rejected before the divide, so it is not offered here.
    j.du = 1u + (rnd(&s) % 4000u);
    j.dv = 1u + (rnd(&s) % 4000u);
    // Numerator strictly below 64*D, which is the precondition that makes six
    // steps sufficient. Generating above it would test the CALLER's clamp.
    j.nu = rnd(&s) % (64u * j.du);
    j.nv = rnd(&s) % (64u * j.dv);
    j.tag = static_cast<uint8_t>(i & 0xFF);
    j.qu = static_cast<uint32_t>(j.nu / j.du);
    j.qv = static_cast<uint32_t>(j.nv / j.dv);
    jobs.push_back(j);
  }

  // Include the edges explicitly rather than hoping random hits them.
  for (uint64_t d : {1ull, 2ull, 4095ull}) {
    Job lo{0, d, 0, d, 0xEE, 0, 0};
    jobs.push_back(lo);
    Job hi{63ull * d, d, 63ull * d, d, 0xEF, 63, 63};
    jobs.push_back(hi);
  }

  // ---- feed ONE PER CLOCK and collect -------------------------------------
  std::deque<Job> expect;
  size_t fed = 0, got = 0;
  int bad_q = 0, bad_tag = 0, max_occ = 0;

  const int total = static_cast<int>(jobs.size());
  for (int c = 0; c < total + kLatency + 16; ++c) {
    const bool feeding = fed < jobs.size();
    top.in_valid_i = feeding;
    if (feeding) {
      const Job& j = jobs[fed];
      top.in_ru_i = j.nu;
      top.in_du_i = j.du;
      top.in_rv_i = j.nv;
      top.in_dv_i = j.dv;
      top.in_tag_i = j.tag;
    }
    top.eval();

    if (top.out_valid_o) {
      if (expect.empty()) {
        ++bad_q;
      } else {
        const Job e = expect.front();
        expect.pop_front();
        if (top.out_qu_o != e.qu || top.out_qv_o != e.qv) ++bad_q;
        if (top.out_tag_o != e.tag) ++bad_tag;
        ++got;
      }
    }
    if (top.occupancy_o > max_occ) max_occ = top.occupancy_o;

    if (feeding) {
      expect.push_back(jobs[fed]);
      ++fed;
    }
    zhao::tick(top);
  }
  top.in_valid_i = 0;

  zhao::check(fed == jobs.size(), "every job was offered", jobs.size(), fed);
  zhao::check(got == jobs.size(), "and every job came back exactly once",
              jobs.size(), got);
  zhao::check(bad_q == 0, "every quotient matches n/d computed independently",
              0, bad_q);
  zhao::check(bad_tag == 0, "and each answer carries its own request's tag", 0,
              bad_tag);

  // THE THROUGHPUT CLAIM, asserted rather than described.
  zhao::check(max_occ == kLatency,
              "six requests are in flight simultaneously (II=1, not serial)",
              kLatency, max_occ);
  zhao::check(top.issued_o == jobs.size(), "the issue counter agrees",
              jobs.size(), top.issued_o);

  // ---- a bubble must not corrupt the pipe --------------------------------
  // Gaps in the input are normal; a stage that mistakes a bubble for data
  // produces answers nobody asked for.
  {
    top.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);

    int outs = 0;
    for (int c = 0; c < 40; ++c) {
      const bool feed = (c == 0 || c == 5 || c == 11);
      top.in_valid_i = feed;
      // 100/7 = 14 and 200/4 = 50, both inside [0,63].
      // THE FIRST DRAFT USED dv=3, so 200 >= 64*3 = 192 -- violating the very
      // precondition this file's header states, and the RTL correctly
      // saturated at 63 while the oracle expected 66. The test was wrong.
      // Worth leaving written down: a stimulus that breaks the contract it
      // documents fails as loudly as a broken design and looks identical.
      top.in_ru_i = 100;
      top.in_du_i = 7;
      top.in_rv_i = 200;
      top.in_dv_i = 4;
      top.in_tag_i = 0x5A;
      top.eval();
      if (top.out_valid_o) {
        ++outs;
        if (top.out_qu_o != (100u / 7u) || top.out_qv_o != (200u / 4u)) ++bad_q;
      }
      zhao::tick(top);
    }
    zhao::check(outs == 3, "three sparse requests produce exactly three answers",
                3, outs);
    zhao::check(bad_q == 0, "and bubbles between them produce none", 0, bad_q);
  }

  return zhao::report_and_exit("texture_aux_div6_directed");
}
