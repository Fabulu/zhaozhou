// rsp_dispatch_meta_directed.cpp
//
// ---------------------------------------------------------------------------
// THE DISPATCHER'S METADATA PAYLOAD, AT THE LEAF
// ---------------------------------------------------------------------------
// Packet C widened `zhao_texture_rsp_dispatch` with an optional metadata
// payload: `rsp_meta_i` in, one `*_meta_o` per class out, carried in `cq_m`
// beside the existing data and token. Guarded by `META_EN` so the ORACLE island
// -- gate 3's reference -- elaborates exactly as before.
//
// This is built with META_EN=1. The default-off case is covered by the existing
// `texture_rsp_dispatch_directed`, which still passes, and by the oracle itself.
//
// WHAT THIS CHECKS THAT THE COMPOSED TEST CANNOT
// ----------------------------------------------
// In the island, a metadata disagreement has several possible homes: the bank,
// the write event, the queue, or the reader. Packet C's bilinear investigation
// spent a round eliminating three of them. At the leaf there is only one
// candidate, so a pass here REMOVES the dispatcher from that list for good --
// which is the point of a leaf test and the reason the composed run is not a
// substitute.
//
// Each response carries a metadata word that ENCODES ITS OWN IDENTITY, so a
// payload delivered to the wrong class, or paired with the wrong token, is
// visible rather than merely "different".
#include "Vzhao_texture_rsp_dispatch.h"

#include <cstdint>
#include <cstdio>
#include <deque>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

namespace {

void tick(Vzhao_texture_rsp_dispatch* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

struct Expect {
  uint32_t tok;
  uint64_t meta;
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_rsp_dispatch* d = new Vzhao_texture_rsp_dispatch;

  d->clk = 0;
  d->rst_n = 0;
  d->rsp_valid_i = 0;
  d->clut_ready_i = 1;
  d->near_ready_i = 1;
  d->bil_ready_i = 1;
  d->err_ready_i = 1;
  uint32_t rng = 0xC0FFEEu;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  std::deque<Expect> want[4];
  int sent = 0, got[4] = {0, 0, 0, 0};
  int meta_wrong = 0, tok_wrong = 0, foreign = 0;
  const int kN = 240;

  for (int cyc = 0; cyc < 20000; ++cyc) {
    const bool more = sent < kN;
    if (more) {
      const uint32_t cls = static_cast<uint32_t>(sent & 3);
      const uint32_t tok = (cls << 16) | (0x1000u + sent);
      // The metadata encodes the token, so a payload that arrives beside the
      // wrong response is caught -- not just a payload that is wrong.
      const uint64_t meta = (static_cast<uint64_t>(0xA5u) << 32) | tok;
      d->rsp_valid_i = 1;
      d->rsp_data_i = 0xDEADBEEFu + sent;
      d->rsp_tok_i = tok;
      d->rsp_class_i = cls;
      d->rsp_meta_i = meta;
    } else {
      d->rsp_valid_i = 0;
    }
    d->eval();
    const bool acc = d->rsp_valid_i && d->rsp_ready_o;

    // BACKPRESSURE ON EVERY CLASS LANE. The first version of this test tied all
    // four ready lines high, so a queued record was always read out on the
    // cycle it became valid -- and a metadata word that failed to HOLD beside
    // its response while the lane stalled would never have been noticed.
    //
    // That matters more now than it would have before: the metadata rides the
    // queue as a third payload, and `cq_m` is indexed by the same read pointer
    // as `cq_d`/`cq_t`. If it were ever indexed by anything else, only a stalled
    // lane would show it.
    rng = rng * 1664525u + 1013904223u;
    d->clut_ready_i = ((rng >> 13) & 3u) != 0u;
    d->near_ready_i = ((rng >> 17) & 3u) != 0u;
    d->bil_ready_i  = ((rng >> 21) & 3u) != 0u;
    d->err_ready_i  = ((rng >> 25) & 3u) != 0u;
    d->eval();

    struct Lane { bool v; uint32_t tok; uint64_t meta; };
    const Lane lanes[4] = {
        {d->clut_valid_o != 0, d->clut_tok_o, d->clut_meta_o},
        {d->near_valid_o != 0, d->near_tok_o, d->near_meta_o},
        {d->bil_valid_o  != 0, d->bil_tok_o,  d->bil_meta_o},
        {d->err_valid_o  != 0, d->err_tok_o,  d->err_meta_o}};
    const bool rdy[4] = {d->clut_ready_i != 0, d->near_ready_i != 0,
                         d->bil_ready_i != 0, d->err_ready_i != 0};
    for (int c = 0; c < 4; ++c) {
      if (!lanes[c].v || !rdy[c]) continue;
      ++got[c];
      if (want[c].empty()) { ++foreign; continue; }
      const Expect e = want[c].front();
      want[c].pop_front();
      if (lanes[c].tok != e.tok) ++tok_wrong;
      if (lanes[c].meta != e.meta) ++meta_wrong;
    }
    // PUSH AFTER SAMPLING, not before. Pushing first let a response accepted
    // THIS cycle be matched against a lane output that belongs to an earlier
    // one, which is a defect in this harness and not in the dispatcher --
    // the first version of this test reported 180 token errors because of it.
    if (acc) {
      const uint32_t cls = static_cast<uint32_t>(sent & 3);
      const uint32_t tok = (cls << 16) | (0x1000u + sent);
      want[cls].push_back({tok, (static_cast<uint64_t>(0xA5u) << 32) | tok});
    }
    tick(d);
    if (acc) ++sent;
    if (sent >= kN && want[0].empty() && want[1].empty()
        && want[2].empty() && want[3].empty()) break;
  }

  std::printf("  dispatched %d | clut %d near %d bil %d err %d\n",
              sent, got[0], got[1], got[2], got[3]);

  zhao::check(sent == kN,
              "every response was accepted despite randomised backpressure on "
              "all four class lanes",
              kN, sent);
  zhao::check(got[0] > 0 && got[1] > 0 && got[2] > 0 && got[3] > 0,
              "ALL FOUR class lanes carried traffic -- a metadata check that "
              "only exercised one lane is what let packet C move two readers on "
              "evidence from a third",
              1, (got[0] && got[1] && got[2] && got[3]) ? 1 : 0);
  zhao::check(foreign == 0, "no lane produced a response nobody sent", 0, foreign);
  zhao::check(tok_wrong == 0, "every token arrived on its own lane in order",
              0, tok_wrong);
  zhao::check(meta_wrong == 0,
              "and EVERY metadata word arrived beside the response it belongs "
              "to. The payload encodes its own token, so a word delivered to "
              "the wrong lane or paired with the wrong response is caught -- "
              "not merely a word that differs",
              0, meta_wrong);

  const int rc = zhao::report_and_exit("rsp_dispatch_meta_directed");
  delete d;
  zhao::exit_hard(rc);
}
