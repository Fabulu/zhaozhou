// frag_expand_directed.cpp
//
// ---------------------------------------------------------------------------
// STAGE B's ACCEPTANCE TEST: THE SEQUENCE, AND THE COUNTS
// ---------------------------------------------------------------------------
// The architecture's falsifier for this block is explicit and it is not "the
// output looks right":
//
//   "any request divergence; any iss pulse without an accepted request --
//    assert exact per-fragment issue counts, not just output equality (the
//    counters-see-what-pictures-cannot law: byte-identical output has twice
//    hidden a machine doing double work)."
//
// So this checks the REQUEST SEQUENCE element for element against a model of
// fragrob's expansion, and it checks the per-fragment issue COUNT exactly --
// not >=, not "eventually".
//
// The contract being reproduced is written out in
// reports/P0C-STAGEB-EXPANDER-CONTRACT-20260908.md.
#include "Vzhao_texture_frag_expand.h"

#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

namespace {

struct Frag {
  uint16_t owner;   // {slot[5:0], gen[7:0]}
  int32_t u, v;
  uint8_t binding, lod;
  uint8_t count;    // 0..3
  bool aux;
  uint8_t cls;
  uint64_t ctx;     // the caller's opaque context; the AUX request's world
                    // coordinates live in its low 64 bits
};

struct Req {
  uint32_t src_id;
  int32_t u, v;
  uint8_t lod;
};

// fragrob.sv:359-367's table, reproduced here so a divergence is a test
// failure rather than a shared bug.
int expected_requests(uint8_t count) { return count > 3 ? 3 : count; }

void tick(Vzhao_texture_frag_expand* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

}  // namespace

int main() {
  Vzhao_texture_frag_expand* d = new Vzhao_texture_frag_expand;

  d->rst_n = 0;
  d->f_valid_i = 0;
  d->req_ready_i = 0;
  d->aux_ready_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  // ---- the workload --------------------------------------------------------
  // Every sample count including ZERO, aux on and off, distinct owners so a
  // mis-routed request is visible, and enough fragments that the queue wraps.
  std::vector<Frag> work;
  uint32_t st = 0x1234u;
  for (int i = 0; i < 64; ++i) {
    Frag f;
    f.owner = static_cast<uint16_t>(((i & 0x3F) << 8) | ((i * 7) & 0xFF));
    f.u = static_cast<int32_t>(0x10000 + i * 137);
    f.v = static_cast<int32_t>(0x20000 - i * 91);
    f.binding = static_cast<uint8_t>(i & 0xFF);
    f.lod = static_cast<uint8_t>((i * 3) & 0xFF);
    f.count = static_cast<uint8_t>(i % 4);   // 0,1,2,3 cycling -- ZERO included
    f.aux = (i % 3) == 0;
    f.cls = static_cast<uint8_t>(i & 3);
    // Distinct per fragment, so an aux request carrying the WRONG fragment's
    // context is visible rather than accidentally equal.
    f.ctx = (static_cast<uint64_t>(0xC0DE0000u + i) << 32) |
            static_cast<uint64_t>(0x1000u + i * 37);
    work.push_back(f);
    st = st * 1664525u + 1013904223u;
  }

  // The model: fragrob's sequence, ascending sidx within a fragment, FIFO
  // across fragments.
  std::deque<Req> expect;
  std::deque<uint64_t> expect_aux_ctx;
  int expect_aux = 0, expect_zero = 0;
  for (const Frag& f : work) {
    const int n = expected_requests(f.count);
    if (n == 0) ++expect_zero;
    for (int s = 0; s < n; ++s) {
      Req r;
      r.src_id = (static_cast<uint32_t>(f.cls) << 16) |
                 (static_cast<uint32_t>((f.owner >> 8) & 0x3F) << 10) |
                 (static_cast<uint32_t>(s) << 8) |
                 (f.owner & 0xFF);
      r.u = f.u;
      r.v = f.v;
      r.lod = f.lod;
      expect.push_back(r);
    }
    if (f.aux) { ++expect_aux; expect_aux_ctx.push_back(f.ctx); }
  }

  // ---- drive ---------------------------------------------------------------
  size_t next_in = 0;
  int got_reqs = 0, seq_errors = 0, got_aux = 0;
  int iss_without_fire = 0, iss_handle_errors = 0, aux_ctx_errors = 0;
  uint32_t rng = 0xBEEFu;

  for (int cyc = 0; cyc < 20000 && (!expect.empty() || next_in < work.size()); ++cyc) {
    rng = rng * 1664525u + 1013904223u;
    // Backpressure on both sinks, so the expander is exercised stalled as well
    // as free-running.
    d->req_ready_i = ((rng >> 13) & 3u) != 0u;
    d->aux_ready_i = ((rng >> 19) & 1u) != 0u;

    if (next_in < work.size()) {
      const Frag& f = work[next_in];
      d->f_valid_i = 1;
      d->f_owner_i = f.owner;
      d->f_u_i = f.u;
      d->f_v_i = f.v;
      d->f_binding_i = f.binding;
      d->f_lod_i = f.lod;
      d->f_count_i = f.count;
      d->f_aux_i = f.aux ? 1 : 0;
      d->f_class_i = f.cls;
      d->f_ctx_i = f.ctx;
    } else {
      d->f_valid_i = 0;
    }
    d->eval();

    const bool tmu_fire = d->req_valid_o && d->req_ready_i;
    const bool aux_fire = d->aux_valid_o && d->aux_ready_i;
    const bool accepted = d->f_valid_i && d->f_ready_o;

    if (tmu_fire) {
      ++got_reqs;
      if (expect.empty()) {
        ++seq_errors;
      } else {
        const Req& e = expect.front();
        if (d->req_src_id_o != e.src_id || d->req_u_o != e.u ||
            d->req_v_o != e.v || d->req_lod_o != e.lod)
          ++seq_errors;
        expect.pop_front();
      }
      // The notification must accompany the accepted request and carry the
      // matching sample handle.
      if (!d->iss_tmu_valid_o) ++iss_without_fire;
      const uint16_t want_handle = static_cast<uint16_t>(d->req_src_id_o & 0xFFFF);
      if (d->iss_tmu_handle_o != want_handle) ++iss_handle_errors;
    } else if (d->iss_tmu_valid_o) {
      // An issue pulse with no accepted request is the exact defect the
      // architecture's falsifier names.
      ++iss_without_fire;
    }
    if (aux_fire) {
      ++got_aux;
      // THE CONTEXT MUST BE THIS FRAGMENT'S. Sourcing it from another stage is
      // the defect that made the island ask the sheet about the wrong
      // fragment's world position, and it is invisible in a request COUNT.
      if (expect_aux_ctx.empty()) {
        ++aux_ctx_errors;
      } else {
        if (d->aux_ctx_o != expect_aux_ctx.front()) ++aux_ctx_errors;
        expect_aux_ctx.pop_front();
      }
    }

    if (accepted) ++next_in;
    tick(d);
  }
  d->f_valid_i = 0;
  d->eval();

  std::printf("  requests %d expected %d | aux %d expected %d | zero-sample %u\n",
              got_reqs, static_cast<int>(got_reqs + expect.size()), got_aux,
              expect_aux, d->zero_sample_fragments_o);

  zhao::check(next_in == work.size(),
              "every fragment was accepted -- the run is not truncated", 1,
              next_in == work.size() ? 1 : 0);
  zhao::check(expect.empty(),
              "the expander produced EVERY request the model expected -- none "
              "missing",
              0, static_cast<uint64_t>(expect.size()));
  zhao::check(seq_errors == 0,
              "and the request SEQUENCE matches element for element: same "
              "src_id, same u/v, same lod, ascending sidx within a fragment, "
              "FIFO across fragments",
              0, static_cast<uint64_t>(seq_errors));
  zhao::check(iss_without_fire == 0,
              "no ISSUE pulse without an accepted request -- §11.1's "
              "distinction between an intent and a taken request",
              0, static_cast<uint64_t>(iss_without_fire));
  zhao::check(iss_handle_errors == 0,
              "and every issue handle matches its request's identity", 0,
              static_cast<uint64_t>(iss_handle_errors));
  zhao::check(aux_ctx_errors == 0,
              "every AUX request carries ITS OWN fragment's context, in FIFO "
              "order -- the world coordinates travel with the fragment instead "
              "of being read off whatever another stage is holding",
              0, static_cast<uint64_t>(aux_ctx_errors));
  zhao::check(got_aux == expect_aux,
              "exactly as many AUX requests as fragments that asked for one",
              expect_aux, got_aux);

  // ---- THE COUNTS, which output equality cannot see ------------------------
  zhao::check(d->requests_o == static_cast<uint32_t>(got_reqs),
              "the block's own request counter agrees with what was observed "
              "on the wire -- a machine doing double work would disagree here "
              "while its output still matched",
              static_cast<uint64_t>(got_reqs), d->requests_o);
  zhao::check(d->fragments_o == work.size(),
              "and it counted exactly the fragments it accepted", work.size(),
              d->fragments_o);

  // ---- the zero-sample fragment, exercised and asserted --------------------
  zhao::check(expect_zero > 0,
              "the workload actually contains zero-sample fragments -- the case "
              "an implementation is most likely to get wrong is not vacuous",
              1, expect_zero > 0 ? 1 : 0);
  zhao::check(d->zero_sample_fragments_o == static_cast<uint32_t>(expect_zero),
              "a count-0 fragment is ACCEPTED and issues NO request -- v3own's "
              "§9.1 zero-work owner has to see the same thing",
              static_cast<uint64_t>(expect_zero), d->zero_sample_fragments_o);

  // THE QUEUE-STATE MONITOR MUST BE SILENT IN CORRECT OPERATION, and must be
  // able to speak. Post-fit brief §2.2 rejected the first version of this
  // monitor as algebraically false: it tested `accept_c && fq_full_c`, which
  // substitutes to `f_valid_i && !fq_full_c && fq_full_c`.
  //
  // The replacement watches the extended pointer difference exceeding the depth
  // the queue owns -- a state violation, derived without reference to
  // `f_ready_o`. The brief's own discriminating mutation is `>=` -> `>` in
  // `fq_full_c`, which admits a fifth entry: the OLD monitor could not fire on
  // it, because the bug moved acceptance and detection together. This one can.
  zhao::check(d->wq_overflow_o == 0,
              "the queue-state monitor is silent under correct operation -- "
              "occupancy never exceeded the depth the queue owns",
              0, d->wq_overflow_o);

  const int rc = zhao::report_and_exit("frag_expand_directed");
  delete d;
  zhao::exit_hard(rc);
}
