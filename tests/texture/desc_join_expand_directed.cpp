// desc_join_expand_directed.cpp
//
// ---------------------------------------------------------------------------
// PACKET 2's ACCEPTANCE TEST, RUN BEFORE PACKET 2
// ---------------------------------------------------------------------------
// bank -> join -> the REAL zhao_texture_frag_expand.
//
// Packet 2 rewires the island so the expander and Mosaic are fed exclusively
// from the captured descriptor. When that edit lands, the composed suite runs
// and the only useful question on a failure is "wiring, or modules?". With no
// composed test of the modules themselves that answer costs a bisect through a
// 2,900-line file.
//
// So the chain is proven here first, while the island is still untouched.
// Afterwards, red means wiring — by elimination.
//
// WHAT IT ACTUALLY CHECKS, and why the request SEQUENCE is the right observable:
// the expander emits `count` requests per fragment with ascending sidx, each
// carrying `{class, slot, sidx, gen}` and the fragment's LOD and U/V. Every one
// of those fields except U/V comes from the DESCRIPTOR. So a fragment fed the
// wrong owner's descriptor produces the wrong number of requests, or the right
// number with the wrong LOD, and either shows up element for element.
#include "Vtb_desc_join_expand.h"

#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

namespace {

void tick(Vtb_desc_join_expand* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

struct Desc {
  uint64_t ctx;
  uint8_t gen, lod, cls, aux, count, pslot, pgen, mosa, mosb, mosw, bsel;
};

Desc make_desc(int slot) {
  Desc d;
  d.ctx = (0xFACE0000ull + static_cast<uint64_t>(slot) * 0x3333) << 32 |
          (0x1DEA0000ull + static_cast<uint64_t>(slot) * 17);
  d.gen = static_cast<uint8_t>(0x30 + slot);
  d.lod = static_cast<uint8_t>(slot * 5 + 2);
  // Class cycles 0..3, so CLS_ERR (3) is exercised and the read-point
  // sanitisation to CLS_NEAR (1) is actually reached rather than assumed.
  d.cls = static_cast<uint8_t>(slot & 3);
  d.aux = static_cast<uint8_t>((slot >> 1) & 1);
  d.count = static_cast<uint8_t>(slot & 3);  // includes ZERO
  d.pslot = static_cast<uint8_t>((slot + 1) & 3);
  d.pgen = static_cast<uint8_t>(0xA0 + slot);
  d.mosa = static_cast<uint8_t>(slot * 3 + 11);
  d.mosb = static_cast<uint8_t>(slot * 9 + 5);
  d.mosw = static_cast<uint8_t>(240 - slot);
  d.bsel = static_cast<uint8_t>(slot ^ 0x27);
  return d;
}

uint8_t sane_class(uint8_t c) { return c == 3 ? 1 : c; }

void load(Vtb_desc_join_expand* d, int slot, const Desc& r) {
  d->wr_valid_i = 1;
  d->wr_slot_i = static_cast<uint8_t>(slot);
  d->wr_owner_gen_i = r.gen;
  d->wr_aux_context_i = r.ctx;
  d->wr_lod_i = r.lod;
  d->wr_raw_class_i = r.cls;
  d->wr_needs_aux_i = r.aux;
  d->wr_sample_count_i = r.count;
  d->wr_palette_slot_i = r.pslot;
  d->wr_palette_gen_i = r.pgen;
  d->wr_mosaic_mat_a_i = r.mosa;
  d->wr_mosaic_mat_b_i = r.mosb;
  d->wr_mosaic_weight_i = r.mosw;
  d->wr_binding_sel_i = r.bsel;
  d->eval();
  tick(d);
  d->wr_valid_i = 0;
  d->eval();
}

struct Req {
  uint32_t src_id;
  int32_t u, v;
  uint8_t lod;
  // PACKET 3 / §6: the palette pair as the EXPANDER should emit it. Modelled
  // here rather than checked at the join, because the join is only the first of
  // four hops -- descriptor, join, expander input queue, current-fragment
  // record -- and a pair that is right at hop one proves nothing about hop four.
  uint8_t pslot, pgen;
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_desc_join_expand* d = new Vtb_desc_join_expand;

  d->clk = 0;
  d->rst_n = 0;
  d->wr_valid_i = 0;
  d->p_valid_i = 0;
  d->req_ready_i = 0;
  d->aux_ready_i = 0;
  d->m_ready_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  for (int s = 0; s < 64; ++s) load(d, s, make_desc(s));

  // ---- the workload --------------------------------------------------------
  const int kFragments = 256;
  std::deque<Req> expect;
  std::deque<uint64_t> expect_aux_ctx;
  int expect_aux = 0, expect_zero = 0, expect_reqs = 0;

  int fed = 0, got_reqs = 0, got_aux = 0;
  int seq_errors = 0, aux_ctx_errors = 0, mosaic_errors = 0, took_m = 0;
  uint32_t rng = 0x1BADB002u;

  for (int cyc = 0; cyc < 200000 && (fed < kFragments || !expect.empty()); ++cyc) {
    rng = rng * 1664525u + 1013904223u;
    // All three sinks stalled independently and unevenly.
    d->req_ready_i = ((rng >> 7) & 3u) != 0u;
    d->aux_ready_i = ((rng >> 15) & 1u) != 0u;
    d->m_ready_i = ((rng >> 23) & 7u) != 0u;

    const int slot = fed % 64;
    const Desc r = make_desc(slot);
    if (fed < kFragments) {
      d->p_valid_i = 1;
      d->p_tag_i = static_cast<uint16_t>((slot << 8) | r.gen);
      d->p_u_i = static_cast<int32_t>(0x02000000 + fed * 131);
      d->p_v_i = static_cast<int32_t>(0x03000000 - fed * 197);
      d->p_sat_i = 0;
      d->p_dz_i = 0;
    } else {
      d->p_valid_i = 0;
    }
    d->eval();

    if (d->p_valid_i && d->p_ready_o) {
      // The model, built from THIS fragment's own descriptor.
      const int n = r.count;
      if (n == 0) ++expect_zero;
      for (int s2 = 0; s2 < n; ++s2) {
        Req q;
        q.src_id = (static_cast<uint32_t>(sane_class(r.cls)) << 16) |
                   (static_cast<uint32_t>(slot & 0x3F) << 10) | (static_cast<uint32_t>(s2) << 8) |
                   r.gen;
        q.u = static_cast<int32_t>(d->p_u_i);
        q.v = static_cast<int32_t>(d->p_v_i);
        q.lod = r.lod;
        q.pslot = r.pslot;
        q.pgen = r.pgen;
        expect.push_back(q);
        ++expect_reqs;
      }
      if (r.aux) {
        ++expect_aux;
        expect_aux_ctx.push_back(r.ctx);
      }
      ++fed;
    }

    if (d->req_valid_o && d->req_ready_i) {
      ++got_reqs;
      if (expect.empty()) {
        ++seq_errors;
      } else {
        const Req& e = expect.front();
        if (d->req_src_id_o != e.src_id || d->req_u_o != e.u || d->req_v_o != e.v ||
            d->req_lod_o != e.lod || d->req_pal_slot_o != e.pslot || d->req_pal_gen_o != e.pgen)
          ++seq_errors;
        expect.pop_front();
      }
    }
    if (d->aux_valid_o && d->aux_ready_i) {
      ++got_aux;
      if (expect_aux_ctx.empty()) {
        ++aux_ctx_errors;
      } else {
        if (d->aux_ctx_o != expect_aux_ctx.front()) ++aux_ctx_errors;
        expect_aux_ctx.pop_front();
      }
    }
    if (d->m_valid_o && d->m_ready_i) {
      ++took_m;
      // Mosaic's material bytes must belong to the record being retired, and
      // the join retires in order, so the expected slot is derivable.
      const Desc mr = make_desc((took_m - 1) % 64);
      if (d->m_mat_a_o != mr.mosa || d->m_mat_b_o != mr.mosb || d->m_weight_o != mr.mosw)
        ++mosaic_errors;
    }
    tick(d);
  }
  d->p_valid_i = 0;
  d->eval();

  std::printf("  fed %d | requests %d of %d expected | aux %d of %d | mosaic %d\n", fed, got_reqs,
              expect_reqs, got_aux, expect_aux, took_m);
  std::printf("  errors: sequence %d, aux ctx %d, mosaic %d | leftover expected %d\n", seq_errors,
              aux_ctx_errors, mosaic_errors, static_cast<int>(expect.size()));
  std::printf(
      "  counters: joined %u, desc reads %u, exp fragments %u, exp requests %u, "
      "zero-sample %u, overflow %u, gen mismatch %u\n",
      d->joined_o, d->desc_reads_o, d->exp_fragments_o, d->exp_requests_o, d->exp_zero_o,
      d->exp_overflow_o, d->gen_mismatch_o);

  zhao::check(fed == kFragments, "every fragment was accepted", kFragments, fed);
  zhao::check(expect.empty(),
              "the expander produced EVERY request the descriptor chain implies -- "
              "none missing",
              0, static_cast<uint64_t>(expect.size()));
  zhao::check(seq_errors == 0,
              "and the request SEQUENCE matches element for element: src_id built "
              "from the CAPTURED class/slot/gen, the descriptor's LOD, and the U/V "
              "that arrived with that fragment. A fragment fed another owner's "
              "descriptor would show the wrong LOD or the wrong count here",
              0, static_cast<uint64_t>(seq_errors));
  zhao::check(aux_ctx_errors == 0,
              "every AUX request carries its OWN fragment's 64-bit context, in "
              "order -- read from the bank's held record, not from whatever stage "
              "is currently holding a context",
              0, static_cast<uint64_t>(aux_ctx_errors));
  zhao::check(got_aux == expect_aux, "exactly as many AUX requests as descriptors asked for",
              expect_aux, got_aux);
  zhao::check(mosaic_errors == 0, "and Mosaic's material bytes belong to the record being retired",
              0, static_cast<uint64_t>(mosaic_errors));

  // Non-vacuity: the interesting cases must actually occur.
  zhao::check(expect_zero > 0,
              "zero-sample fragments were exercised -- the case an expansion is "
              "most likely to get wrong",
              1, expect_zero > 0 ? 1 : 0);
  zhao::check(expect_aux > 0, "and AUX-requesting fragments", 1, expect_aux > 0 ? 1 : 0);

  // ---- counters, which output equality cannot see --------------------------
  zhao::check(d->exp_fragments_o == static_cast<uint32_t>(kFragments),
              "the expander counted exactly the fragments the join handed it", kFragments,
              d->exp_fragments_o);
  zhao::check(d->exp_requests_o == static_cast<uint32_t>(expect_reqs),
              "and exactly the requests the descriptors imply -- a machine "
              "reissuing would match on output and disagree only here",
              static_cast<uint64_t>(expect_reqs), d->exp_requests_o);
  zhao::check(d->exp_zero_o == static_cast<uint32_t>(expect_zero),
              "count-0 fragments were accepted and issued no request",
              static_cast<uint64_t>(expect_zero), d->exp_zero_o);
  zhao::check(d->desc_reads_o == static_cast<uint32_t>(kFragments),
              "ONE descriptor read per fragment -- no speculative re-reads while "
              "the expander stalls",
              kFragments, d->desc_reads_o);
  zhao::check(d->joined_o == static_cast<uint32_t>(kFragments), "and one join per fragment",
              kFragments, d->joined_o);
  zhao::check(d->exp_overflow_o == 0, "the expander's queue-state monitor stayed silent", 0,
              d->exp_overflow_o);
  zhao::check(d->gen_mismatch_o == 0,
              "and no generation mismatch: every record's token identity matched "
              "the bank's row",
              0, d->gen_mismatch_o);

  const int rc = zhao::report_and_exit("desc_join_expand_directed");
  delete d;
  zhao::exit_hard(rc);
}
