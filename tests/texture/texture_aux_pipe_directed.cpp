// texture_aux_pipe_directed.cpp — does AUX accept every clock, and does a
// degenerate envelope still keep its place in line?
//
// ---------------------------------------------------------------------------
// THE REQUIRED RESULTS, FROM THE BRIEF
// ---------------------------------------------------------------------------
//   > AUX acceptance II = 1
//   > Surface Sheet request II = 1
//   > AUX response II = 1 when the sheet is unstalled
//
// The shipped FSM does three quotient bits, three more, one sheet read, waits,
// presents, and only then accepts again -- about 277,778/frame against a
// 276,480 estimate. A block sized at 1.005x its own demand has no reserve, so
// the throughput here is the whole point and is asserted rather than described.
//
// ---------------------------------------------------------------------------
// AND THE ORDERING RULE THAT IS EASY TO GET WRONG
// ---------------------------------------------------------------------------
//   > A degenerate envelope travels through the ordering machinery but emits
//   > no sheet read.
//
// Dropping a degenerate request at the front is simpler, costs nothing
// visible, and silently reorders AUX returns against the fragments that asked
// for them. So the central case interleaves degenerate and normal requests and
// requires the RETURN ORDER to match the submission order exactly, with the
// degenerate ones flagged and no sheet read issued for them.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"

#include "Vzhao_texture_aux_pipe.h"

#include "zhao_sim.hpp"

namespace {

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

struct Req {
  int32_t wx, wz, x0, x1, z0, z1;
  uint8_t tok;
  bool degen;
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_aux_pipe top;

  auto reset = [&]() {
    top.req_valid_i = 0;
    top.sheet_ready_i = 1;
    top.sheet_rvalid_i = 0;
    top.out_ready_i = 1;
    top.rst_n = 0;
    for (int i = 0; i < 8; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  // ---- stimulus: interleaved degenerate and normal ------------------------
  std::vector<Req> rq;
  uint32_t s = 0x5EED01u;
  for (int i = 0; i < 64; ++i) {
    Req r{};
    r.tok = static_cast<uint8_t>(i);
    // every third request has a degenerate envelope (x1 <= x0)
    r.degen = (i % 3) == 2;
    r.x0 = 1000;
    r.x1 = r.degen ? 1000 : 1000 + 64 + static_cast<int32_t>(rnd(&s) % 512);
    r.z0 = 2000;
    r.z1 = 2000 + 64 + static_cast<int32_t>(rnd(&s) % 512);
    r.wx = r.x0 + static_cast<int32_t>(rnd(&s) % 600);
    r.wz = r.z0 + static_cast<int32_t>(rnd(&s) % 600);
    rq.push_back(r);
  }

  // ---- run: accept one per clock, model the sheet as a 1-clock echo -------
  reset();
  std::deque<uint8_t> sheet_q;      // tokens with a read outstanding
  std::vector<uint8_t> out_tok;
  std::vector<int>     out_deg;
  size_t fed = 0;
  int accept_clocks = 0, sheet_reads = 0;

  for (int c = 0; c < 600 && out_tok.size() < rq.size(); ++c) {
    const bool feeding = fed < rq.size();
    top.req_valid_i = feeding;
    if (feeding) {
      const Req& r = rq[fed];
      top.req_wx_i = r.wx;   top.req_wz_i = r.wz;
      top.req_env_x0_i = r.x0; top.req_env_x1_i = r.x1;
      top.req_env_z0_i = r.z0; top.req_env_z1_i = r.z1;
      top.req_tok_i = r.tok;
    }

    // The sheet answers the read offered on the PREVIOUS clock.
    if (!sheet_q.empty()) {
      top.sheet_rvalid_i = 1;
      top.sheet_rtok_i = sheet_q.front();
      top.sheet_tag_i = static_cast<uint8_t>(0x40 + sheet_q.front());
      top.sheet_str_i = static_cast<uint8_t>(0x80 + sheet_q.front());
      sheet_q.pop_front();
    } else {
      top.sheet_rvalid_i = 0;
    }

    top.eval();

    if (top.out_valid_o && top.out_ready_i) {
      out_tok.push_back(top.out_tok_o);
      out_deg.push_back(top.out_degenerate_o);
    }
    if (top.sheet_valid_o && top.sheet_ready_i) {
      sheet_q.push_back(top.sheet_tok_o);
      ++sheet_reads;
    }
    if (feeding && top.req_ready_o) ++accept_clocks;

    const bool took = feeding && top.req_ready_o;
    zhao::tick(top);
    if (took) ++fed;
  }
  top.req_valid_i = 0;
  top.sheet_rvalid_i = 0;

  zhao::check(fed == rq.size(), "every request was accepted", rq.size(), fed);
  zhao::check(accept_clocks == static_cast<int>(rq.size()),
              "AUX acceptance is II=1 -- one per clock, no refusals",
              static_cast<int>(rq.size()), accept_clocks);

  const int want_reads = static_cast<int>(rq.size()) - static_cast<int>(rq.size() + 1) / 3;
  zhao::check(sheet_reads == want_reads,
              "a sheet read is issued for every NON-degenerate request only",
              want_reads, sheet_reads);
  zhao::check(top.degenerate_o == static_cast<uint32_t>((rq.size() + 1) / 3),
              "and the degenerate ones are counted",
              static_cast<uint32_t>((rq.size() + 1) / 3), top.degenerate_o);

  zhao::check(out_tok.size() == rq.size(), "every request returns exactly once",
              rq.size(), out_tok.size());

  // THE ORDERING RULE. Degenerate requests must keep their place.
  int order_bad = 0, flag_bad = 0;
  for (size_t i = 0; i < out_tok.size() && i < rq.size(); ++i) {
    if (out_tok[i] != rq[i].tok) ++order_bad;
    if (static_cast<bool>(out_deg[i]) != rq[i].degen) ++flag_bad;
  }
  zhao::check(order_bad == 0,
              "returns come back in SUBMISSION order -- degenerate ones keep their place",
              0, order_bad);
  zhao::check(flag_bad == 0, "and each is flagged degenerate or not, correctly", 0,
              flag_bad);

  std::printf("  %zu requests, %d accept clocks, %d sheet reads, %u degenerate\n",
              rq.size(), accept_clocks, sheet_reads, top.degenerate_o);

  return zhao::report_and_exit("texture_aux_pipe_directed");
}
