// texture_rsp_dispatch_directed.cpp — does a busy bilinear lane still block
// everything, or has the coupling actually gone?
//
// ---------------------------------------------------------------------------
// THE ONE CLAIM WORTH TESTING
// ---------------------------------------------------------------------------
// REARCHITECTUREADVICE.md: "A busy bilinear channel lane must not stop an
// unrelated nearest or CLUT response from entering local response storage."
//
// The shipped TMU has `cac_ready_o = !fl_v` -- one busy filter refuses every
// response. A dispatcher that merely SPLITS the stream into three outputs but
// still derives its input ready from all three would pass a routing test
// perfectly and reproduce the defect exactly. So the central case here holds
// the bilinear output permanently stalled and requires CLUT and nearest to
// keep flowing.
//
// The honest limit is tested too: with the raw FIFO head-of-line blocked by a
// full bilinear queue, dispatch DOES stall, and `hol_stall_o` counts it. That
// is a bounded cost of one shared FIFO rather than a hidden one, and a test
// that pretended otherwise would be describing a different design.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_texture_rsp_dispatch.h"

#include "zhao_sim.hpp"

namespace {

constexpr int CLS_CLUT = 0;
constexpr int CLS_NEAR = 1;
constexpr int CLS_BIL = 2;

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_rsp_dispatch top;

  auto reset = [&]() {
    top.rsp_valid_i = 0;
    top.clut_ready_i = 1;
    top.near_ready_i = 1;
    top.bil_ready_i = 1;
    top.rst_n = 0;
    for (int i = 0; i < 6; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  // ---- 1: routing is correct and order WITHIN a class is preserved --------
  {
    reset();
    struct Exp {
      int cls;
      uint32_t tok;
    };
    std::vector<Exp> seq;
    for (int i = 0; i < 24; ++i) seq.push_back({i % 3, static_cast<uint32_t>(100 + i)});

    std::vector<uint32_t> gotc, gotn, gotb;
    size_t fed = 0;
    for (int c = 0; c < 400 && (gotc.size() + gotn.size() + gotb.size()) < seq.size(); ++c) {
      const bool feeding = fed < seq.size();
      top.rsp_valid_i = feeding;
      if (feeding) {
        top.rsp_data_i = 0xA000 + seq[fed].tok;
        top.rsp_tok_i = seq[fed].tok;
        top.rsp_class_i = seq[fed].cls;
      }
      top.eval();
      if (top.clut_valid_o && top.clut_ready_i) gotc.push_back(top.clut_tok_o);
      if (top.near_valid_o && top.near_ready_i) gotn.push_back(top.near_tok_o);
      if (top.bil_valid_o && top.bil_ready_i) gotb.push_back(top.bil_tok_o);
      const bool took = feeding && top.rsp_ready_o;
      zhao::tick(top);
      if (took) ++fed;
    }
    top.rsp_valid_i = 0;

    zhao::check(gotc.size() + gotn.size() + gotb.size() == seq.size(),
                "every response comes out exactly once", seq.size(),
                gotc.size() + gotn.size() + gotb.size());

    bool order_ok = true;
    size_t ic = 0, in = 0, ib = 0;
    for (const Exp& e : seq) {
      if (e.cls == CLS_CLUT && (ic >= gotc.size() || gotc[ic++] != e.tok)) order_ok = false;
      if (e.cls == CLS_NEAR && (in >= gotn.size() || gotn[in++] != e.tok)) order_ok = false;
      if (e.cls == CLS_BIL && (ib >= gotb.size() || gotb[ib++] != e.tok)) order_ok = false;
    }
    zhao::check(order_ok, "and order WITHIN each class is preserved", 1, order_ok ? 1 : 0);
  }

  // ---- 2: THE CLAIM -- a stalled bilinear lane must not block the others --
  {
    reset();
    top.bil_ready_i = 0;  // the filter is busy, permanently
    top.clut_ready_i = 1;
    top.near_ready_i = 1;

    int clut_out = 0, near_out = 0, accepted = 0;
    for (int c = 0; c < 200; ++c) {
      // Alternate CLUT and NEAR only -- no bilinear offered, so the bilinear
      // queue never fills. This isolates "does the SHARED ready depend on the
      // bilinear lane" from the head-of-line case tested next.
      top.rsp_valid_i = 1;
      top.rsp_data_i = 0xB000 + c;
      top.rsp_tok_i = c;
      top.rsp_class_i = (c & 1) ? CLS_NEAR : CLS_CLUT;
      top.eval();
      if (top.clut_valid_o && top.clut_ready_i) ++clut_out;
      if (top.near_valid_o && top.near_ready_i) ++near_out;
      if (top.rsp_ready_o) ++accepted;
      zhao::tick(top);
    }
    top.rsp_valid_i = 0;

    zhao::check(accepted > 150, "with the BILINEAR lane stalled, responses are still accepted", 1,
                accepted > 150 ? 1 : 0);
    zhao::check(clut_out > 50 && near_out > 50, "and CLUT and NEAREST both keep draining", 1,
                (clut_out > 50 && near_out > 50) ? 1 : 0);
    std::printf("  bilinear stalled: accepted %d, clut out %d, near out %d\n", accepted, clut_out,
                near_out);
  }

  // ---- 3: the honest limit -- head-of-line IS possible, and is counted ----
  {
    reset();
    top.bil_ready_i = 0;
    // Offer only bilinear until its queue fills, then a CLUT behind it.
    for (int c = 0; c < 20; ++c) {
      top.rsp_valid_i = 1;
      top.rsp_data_i = c;
      top.rsp_tok_i = c;
      top.rsp_class_i = CLS_BIL;
      top.eval();
      zhao::tick(top);
    }
    top.rsp_valid_i = 0;
    top.eval();
    zhao::check(top.hol_stall_o > 0, "a full class queue DOES stall the shared head, and says so",
                1, top.hol_stall_o > 0 ? 1 : 0);
    std::printf("  head-of-line stalls counted: %u\n", top.hol_stall_o);
  }

  return zhao::report_and_exit("texture_rsp_dispatch_directed");
}
