// uv_join_directed.cpp
//
// ---------------------------------------------------------------------------
// THE POST-PERSPUV JOIN, COMPOSED WITH THE REAL DESCRIPTOR BANK
// ---------------------------------------------------------------------------
// Decrufter §5.3's obligations, each one an assertion below:
//
//   "Advance all parts together."                     -> the atomicity sweep
//   "Hold the complete bundle while the expander
//    stalls."                                          -> the stall check
//   "Feed the expander and Mosaic with this
//    descriptor's values, not live ingress values
//    and not another stage's current owner."           -> the interleave check
//   "Any required Mosaic acceptance must be included
//    in the fork's handshake."                         -> the fork checks
//   "Demonstrate one joined result per clock where
//    the consumer can accept it."                      -> the rate check
//
// The interleave check is the one that matters most and is easiest to fake. It
// is not enough to show that a record's descriptor is correct when nothing else
// is happening; the defect class here (D0's, and the one §5.3 exists to close)
// only appears when a LATER transaction is offered while an earlier one is still
// held. So the sweep keeps PERSPUV pushing whenever the join will take it, and
// stalls the consumers at pseudo-random, and every retired record is checked
// against the descriptor its OWN owner slot was loaded with.
#include "Vtb_uv_join_pair.h"

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

namespace {

void tick(Vtb_uv_join_pair* d) {
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
  d.ctx = (0xADD00000ull + static_cast<uint64_t>(slot) * 0x2222) << 32 |
          (0x5EED0000ull + static_cast<uint64_t>(slot) * 13);
  d.gen = static_cast<uint8_t>(0x20 + slot);
  d.lod = static_cast<uint8_t>(slot * 3 + 5);
  d.cls = static_cast<uint8_t>(slot & 3);
  d.aux = static_cast<uint8_t>((slot >> 2) & 1);
  d.count = static_cast<uint8_t>((slot + 1) & 3);
  d.pslot = static_cast<uint8_t>((slot + 3) & 3);
  d.pgen = static_cast<uint8_t>(0x90 + slot);
  d.mosa = static_cast<uint8_t>(slot * 7 + 1);
  d.mosb = static_cast<uint8_t>(slot * 13 + 9);
  d.mosw = static_cast<uint8_t>(200 - slot * 2);
  d.bsel = static_cast<uint8_t>(slot ^ 0x3C);
  return d;
}

void load(Vtb_uv_join_pair* d, int slot, const Desc& r) {
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

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_uv_join_pair* d = new Vtb_uv_join_pair;

  d->clk = 0;
  d->rst_n = 0;
  d->wr_valid_i = 0;
  d->p_valid_i = 0;
  d->f_ready_i = 0;
  d->m_ready_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  // Every owner slot carries a different descriptor in every field, so a record
  // paired with the wrong owner's descriptor cannot alias into looking right.
  for (int s = 0; s < 64; ++s) load(d, s, make_desc(s));

  // =========================================================================
  // THE INTERLEAVE SWEEP
  // =========================================================================
  const int kFragments = 400;
  int fed = 0, took_f = 0, took_m = 0;
  int desc_errors = 0, owner_errors = 0, uv_errors = 0;
  int double_issue_f = 0, double_issue_m = 0;
  uint32_t rng = 0xA17CE55u;

  // What the expander/Mosaic should see, indexed by the order records retire.
  std::vector<int> expect_slot;
  std::vector<int32_t> expect_u, expect_v;
  size_t f_seen = 0, m_seen = 0;

  bool f_open_prev = false, m_open_prev = false;
  int f_fires_this_record = 0, m_fires_this_record = 0;

  for (int cyc = 0; cyc < 20000 && (fed < kFragments || f_seen < expect_slot.size()); ++cyc) {
    rng = rng * 1664525u + 1013904223u;
    // Independent, uneven backpressure on the two branches -- §5.6 asks for the
    // consumers stalled independently, and equal stalling would hide a fork that
    // only works when both are ready on the same cycle.
    d->f_ready_i = ((rng >> 11) & 3u) != 0u;
    d->m_ready_i = ((rng >> 17) & 7u) != 0u;

    const int slot = fed % 64;
    if (fed < kFragments) {
      const Desc r = make_desc(slot);
      d->p_valid_i = 1;
      d->p_tag_i = static_cast<uint16_t>((slot << 8) | r.gen);
      d->p_u_i = static_cast<int32_t>(0x11110000 + fed * 37);
      d->p_v_i = static_cast<int32_t>(0x22220000 - fed * 53);
      d->p_sat_i = (fed % 5) == 0;
      d->p_dz_i = (fed % 7) == 0;
    } else {
      d->p_valid_i = 0;
    }
    d->eval();

    const bool accepted = d->p_valid_i && d->p_ready_o;
    const bool f_fire = d->f_valid_o && d->f_ready_i;
    const bool m_fire = d->m_valid_o && d->m_ready_i;

    if (accepted) {
      expect_slot.push_back(slot);
      expect_u.push_back(static_cast<int32_t>(d->p_u_i));
      expect_v.push_back(static_cast<int32_t>(d->p_v_i));
      ++fed;
    }

    if (f_fire) {
      ++took_f;
      if (f_seen < expect_slot.size()) {
        const int s = expect_slot[f_seen];
        const Desc r = make_desc(s);
        // EVERY descriptor-sourced field, against the descriptor belonging to
        // THIS record's owner -- not the one currently being offered.
        int e = 0;
        if (d->f_ctx_o != r.ctx) ++e;
        if (d->f_lod_o != r.lod) ++e;
        if (d->f_class_o != r.cls) ++e;
        if (d->f_aux_o != r.aux) ++e;
        if (d->f_count_o != r.count) ++e;
        if (d->f_binding_o != r.bsel) ++e;
        // PALETTE IDENTITY, carried rather than looked up. Before D3 step 1
        // the join dropped this pair and the metajoin's write side read
        // `palslot_m`/`palgen_m` by owner slot -- a sidecar lookup keyed on an
        // identity that may already have been recycled. Carried with the
        // request it cannot be stale, and this check is what says so.
        if (d->f_pal_slot_o != r.pslot) ++e;
        if (d->f_pal_gen_o != r.pgen) ++e;
        desc_errors += e;
        if (d->f_owner_o != ((s << 8) | r.gen)) ++owner_errors;
        if (d->f_u_o != expect_u[f_seen] || d->f_v_o != expect_v[f_seen]) ++uv_errors;
        ++f_seen;
      }
      ++f_fires_this_record;
    }
    if (m_fire) {
      ++took_m;
      if (m_seen < expect_slot.size()) {
        const Desc r = make_desc(expect_slot[m_seen]);
        if (d->m_mat_a_o != r.mosa || d->m_mat_b_o != r.mosb || d->m_weight_o != r.mosw)
          ++desc_errors;
        ++m_seen;
      }
      ++m_fires_this_record;
    }

    // A record must be taken exactly once per branch. A sticky done-bit that
    // failed would re-offer it and the machine would do the work twice while
    // producing identical output -- invisible to any result check.
    if (f_fires_this_record > 1) ++double_issue_f;
    if (m_fires_this_record > 1) ++double_issue_m;
    if (accepted) {
      f_fires_this_record = 0;
      m_fires_this_record = 0;
    }

    f_open_prev = d->f_ready_i;
    m_open_prev = d->m_ready_i;
    tick(d);
  }
  d->p_valid_i = 0;
  d->eval();
  (void)f_open_prev;
  (void)m_open_prev;

  std::printf(
      "  interleaved: fed %d, expander took %d, Mosaic took %d | "
      "desc errors %d, owner errors %d, u/v errors %d\n",
      fed, took_f, took_m, desc_errors, owner_errors, uv_errors);
  std::printf(
      "  joined %u, saturated %u, depth-zero %u, gen mismatches %u | "
      "descriptor reads %u writes %u\n",
      d->joined_o, d->saturated_o, d->depth_zero_o, d->gen_mismatch_o, d->desc_reads_o,
      d->desc_writes_o);

  zhao::check(fed == kFragments, "every fragment was accepted", kFragments, fed);
  zhao::check(took_f == kFragments, "the expander received exactly one record per fragment",
              kFragments, took_f);
  zhao::check(took_m == kFragments,
              "and so did Mosaic -- its ready is part of the fork's handshake, "
              "not ignored. A tied-off ready would make an ignored one look "
              "correct until the day Mosaic becomes functional",
              kFragments, took_m);
  zhao::check(desc_errors == 0,
              "every descriptor-sourced field belongs to the record's OWN owner, "
              "under independent random backpressure on both branches with later "
              "fragments offered while earlier ones are held -- §5.3's 'not live "
              "ingress values and not another stage's current owner'",
              0, static_cast<uint64_t>(desc_errors));
  zhao::check(owner_errors == 0, "and the owner handle travels with it", 0,
              static_cast<uint64_t>(owner_errors));
  zhao::check(uv_errors == 0,
              "and PERSPUV's U/V stay with the descriptor they arrived beside -- "
              "the atomicity D0 taught, here by construction: one enable "
              "advances the join's registers and the bank's output register",
              0, static_cast<uint64_t>(uv_errors));
  zhao::check(double_issue_f == 0 && double_issue_m == 0,
              "no record is offered twice to either branch", 0,
              static_cast<uint64_t>(double_issue_f + double_issue_m));
  zhao::check(d->joined_o == static_cast<uint32_t>(kFragments),
              "the join's own counter agrees with the traffic observed on the "
              "wire",
              kFragments, d->joined_o);
  zhao::check(d->desc_reads_o == static_cast<uint32_t>(kFragments),
              "exactly one descriptor read per joined record -- no speculative "
              "re-reads while a consumer stalls, which is what an ungated read "
              "enable would produce",
              kFragments, d->desc_reads_o);
  zhao::check(d->gen_mismatch_o == 0,
              "and no generation mismatch: every record's token identity matched "
              "the bank's row for that slot",
              0, d->gen_mismatch_o);

  // =========================================================================
  // ONE JOINED RESULT PER CLOCK, which §5.3 asks to be DEMONSTRATED
  // =========================================================================
  {
    d->f_ready_i = 1;
    d->m_ready_i = 1;
    int retired = 0;
    const uint32_t before = d->joined_o;
    const int kBurst = 64;
    int clocks = 0;
    for (int i = 0; i < 400 && retired < kBurst; ++i) {
      const int slot = retired % 64;
      const Desc r = make_desc(slot);
      d->p_valid_i = 1;
      d->p_tag_i = static_cast<uint16_t>((slot << 8) | r.gen);
      d->p_u_i = 0x30000000 + retired;
      d->p_v_i = 0x40000000 - retired;
      d->p_sat_i = 0;
      d->p_dz_i = 0;
      d->eval();
      if (d->p_valid_i && d->p_ready_o) ++retired;
      tick(d);
      ++clocks;
    }
    d->p_valid_i = 0;
    d->eval();
    std::printf(
        "  unstalled: %d accepted in %d clocks (%.2f per result), "
        "joined %u -> %u\n",
        retired, clocks, static_cast<double>(clocks) / (retired ? retired : 1), before,
        d->joined_o);
    zhao::check(retired == kBurst, "the burst completed", kBurst, retired);
    zhao::check(clocks <= kBurst + 1,
                "ONE joined result per clock while the consumers accept -- "
                "§5.3's explicit demonstration. The added cycle is a latency "
                "trade and not a throughput one; a join that cost two clocks "
                "per result would show up here as double",
                1, clocks <= kBurst + 1 ? 1 : 0);
  }

  const int rc = zhao::report_and_exit("uv_join_directed");
  delete d;
  zhao::exit_hard(rc);
}
