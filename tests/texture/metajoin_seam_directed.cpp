// metajoin_seam_directed.cpp
//
// ---------------------------------------------------------------------------
// D0: HELD A, OFFERED B — THE RAM/JOIN SEAM
// ---------------------------------------------------------------------------
// The Decrufter brief, §14 step 2: "Reproduce D0's held-A/offered-B failure at
// the RAM-join/dispatcher seam ... Do not use the standalone dispatcher
// backpressure test as a substitute for this seam test."
//
// THE DEFECT, from source. `zhao_texture_metajoin` registers its read
// UNCONDITIONALLY:
//
//     rd_q <= mem_q[rd_addr_c];        // every cycle, from the offered address
//
// The island's join stage holds `r1_d_q`/`r1_t_q` when the dispatcher stalls,
// but the bank's output keeps tracking whatever address is presented next. The
// dispatcher consumes that output directly, so a legal sequence yields
//
//     response A's data + response A's token + response B's METADATA
//
// and every accepted/emitted counter still balances. Nothing downstream can
// notice, because the record is internally consistent in every field the
// counters look at.
//
// WHY THE EXISTING BACKPRESSURE TEST MISSES IT. `rsp_dispatch_meta_directed`
// stalls the four CLASS lanes, which are DOWNSTREAM of the dispatcher's input.
// This seam is upstream of it: bank -> join -> dispatcher. Stalling one does
// not exercise the other, and I reported the dispatcher test's pass as though
// it covered the join. It never touched this boundary.
//
// This test drives the bank directly and holds the read address steady while
// offering a different one, which is the same condition the island produces
// when `disp_rsp_ready` drops.
#include "Vzhao_texture_metajoin.h"

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

namespace {

void tick(Vzhao_texture_metajoin* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

void write_row(Vzhao_texture_metajoin* d, uint8_t slot, uint8_t sidx, uint8_t ogen, uint8_t pslot,
               uint8_t pgen, uint8_t fmt, uint8_t fu, uint8_t fv, uint8_t bsel, uint8_t nib) {
  d->wr_valid_i = 1;
  d->wr_slot_i = slot;
  d->wr_sidx_i = sidx;
  d->wr_owner_gen_i = ogen;
  d->wr_pal_slot_i = pslot;
  d->wr_pal_gen_i = pgen;
  d->wr_format_i = fmt;
  d->wr_frac_u_i = fu;
  d->wr_frac_v_i = fv;
  d->wr_byte_sel_i = bsel;
  d->wr_nibble_i = nib;
  d->eval();
  tick(d);
  d->wr_valid_i = 0;
  d->eval();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_metajoin* d = new Vzhao_texture_metajoin;

  d->clk = 0;
  d->rst_n = 0;
  d->wr_valid_i = 0;
  d->rd_valid_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  // Two rows whose every field differs, so a swap cannot alias.
  //                slot sidx ogen pslot pgen fmt   fu    fv  bsel nib
  write_row(d, /*A*/ 11, 0, 0x21, 1, 0x30, 2, 0x11, 0x22, 0, 0);
  write_row(d, /*B*/ 22, 1, 0x63, 3, 0x7C, 5, 0xEE, 0xDD, 1, 1);

  // ---- launch A's read, then STALL and offer B ------------------------------
  // This is exactly what the island does when `disp_rsp_ready` drops: the join
  // holds A, and the cache presents B at the bank's address port.
  d->rd_valid_i = 1;
  d->rd_slot_i = 11;
  d->rd_sidx_i = 0;
  d->rd_owner_gen_i = 0x21;
  d->eval();
  tick(d);  // A's read is now registered

  // A's metadata must be present here.
  d->eval();
  const unsigned a_pgen_now = d->rd_pal_gen_o;
  const unsigned a_fmt_now = d->rd_format_o;

  // Now the stall: NO new read is launched (rd_valid_i low, as the island's
  // `cache_smp_valid && r1_room_c` would be), but the address port shows B
  // because the cache is presenting the next response.
  d->rd_valid_i = 0;
  d->rd_slot_i = 22;
  d->rd_sidx_i = 1;
  d->rd_owner_gen_i = 0x63;
  d->eval();
  tick(d);
  d->eval();

  std::printf(
      "  after stall with B offered: pal_gen %02X (A=%02X, B=%02X), "
      "format %u (A=%u, B=%u)\n",
      d->rd_pal_gen_o, 0x30, 0x7C, d->rd_format_o, 2, 5);

  zhao::check(a_pgen_now == 0x30 && a_fmt_now == 2,
              "A's metadata was correctly presented before the stall -- "
              "otherwise the check below is about the wrong thing",
              1, (a_pgen_now == 0x30 && a_fmt_now == 2) ? 1 : 0);

  zhao::check(d->rd_pal_gen_o == 0x30,
              "D0: the bank still presents A's palette generation while the "
              "join holds A and the cache offers B. `rd_q` is registered "
              "UNCONDITIONALLY from the offered address, so it follows B and "
              "the dispatcher pairs A's data and token with B's metadata -- "
              "with every accepted/emitted counter balancing",
              0x30, d->rd_pal_gen_o);
  zhao::check(d->rd_format_o == 2, "and A's format, for the same reason", 2, d->rd_format_o);

  // ---- D0b: THE DETECTOR THAT COULD HAVE CAUGHT D0 IS BLINDED BY IT -------
  // The bank carries what looks like exactly the right guard:
  //
  //     if (rd_v_q && (rd_q[OGEN_LO +: GENW] != rd_gen_q))
  //       rd_gen_mismatch_o <= rd_gen_mismatch_o + 1;
  //
  // It cannot fire on D0. `rd_gen_q <= rd_owner_gen_i` is registered by the
  // SAME ungated assignment as `rd_q`, so on the swap both move to B together
  // -- and B's stored generation of course agrees with B's offered one. The two
  // quantities the detector differences are corrupted in LOCKSTEP, which is why
  // a live identity counter sat beside this defect and stayed at zero. Docket
  // M13's cancelling-errors pattern, third instance this session.
  //
  // Measured pre-repair: 0. That observation lives in
  // reports/D0-JOIN-SEAM-REPRODUCED-20260908.md rather than as an assertion,
  // because after the repair there is no swap for it to miss -- asserting "it
  // fires on the swap" would be a check that can only pass while the bug does.
  //
  // What IS asserted is that the detector works at all, on a genuine staleness:
  // read row A while offering a generation that is not A's. Post-repair this is
  // the real thing the counter is for, and it must fire.
  {
    const unsigned before = d->rd_gen_mismatch_o;
    d->rd_valid_i = 1;
    d->rd_slot_i = 11;
    d->rd_sidx_i = 0;
    d->rd_owner_gen_i = 0x99;  // NOT the 0x21 stored in row A
    d->eval();
    tick(d);
    d->rd_valid_i = 0;
    d->eval();
    tick(d);  // the compare is one cycle behind
    d->eval();
    std::printf("  gen-mismatch on a genuine stale read: %u -> %u\n", before, d->rd_gen_mismatch_o);
    zhao::check(d->rd_gen_mismatch_o > before,
                "the owner-generation detector FIRES on a genuine stale "
                "read -- shown to fire, so it is a live detector and not a "
                "counter that has only ever been zero",
                1, d->rd_gen_mismatch_o > before ? 1 : 0);
  }

  // ---- D0c: THE ILLEGAL-KEY DETECTOR, SHOWN TO FIRE ------------------------
  // `rd_legal_c = (rd_sidx_i != 3)`. sidx 3 is unreachable from the expander
  // (fragrob issues sidx 0..2), so this counter has never fired in anger -- and
  // an untested detector is not a detector. Fire it on purpose.
  const unsigned illegal_before = d->rd_illegal_sidx_o;
  const unsigned pgen_before = d->rd_pal_gen_o;
  d->rd_valid_i = 1;
  d->rd_slot_i = 22;
  d->rd_sidx_i = 3;
  d->rd_owner_gen_i = 0x63;
  d->eval();
  tick(d);
  d->eval();

  zhao::check(d->rd_illegal_sidx_o == illegal_before + 1,
              "the illegal-sidx detector FIRES when shown an illegal key -- "
              "it is a live detector and not a counter that has only ever "
              "been zero",
              illegal_before + 1, d->rd_illegal_sidx_o);
  zhao::check(d->rd_result_valid_o == 0, "and the result is marked invalid", 0,
              d->rd_result_valid_o);

  // BUT: the data register still moved. `rd_q` is loaded from the illegal
  // address regardless, and the island wires `mj_meta_packed_c` -- derived
  // straight from these outputs -- into `rsp_meta_i` WITHOUT consulting
  // `rd_result_valid_o`. The only consumer of that valid bit in the island is
  // the migration shadow comparator. So the production treatment of an invalid
  // key is: count it, and use its data anyway.
  std::printf("  after an ILLEGAL read: pal_gen %02X (was %02X), valid %u\n", d->rd_pal_gen_o,
              pgen_before, d->rd_result_valid_o);
  zhao::check(d->rd_pal_gen_o == pgen_before,
              "an ILLEGAL read leaves the metadata outputs alone. It does "
              "not: `rd_q` loads from the illegal address anyway, and the "
              "island consumes the packed record without ever looking at "
              "rd_result_valid_o",
              pgen_before, d->rd_pal_gen_o);

  // ---- D0d: the owner generation never leaves the bank ---------------------
  // The row stores `wr_owner_gen_i` at OGEN_LO and the bank differences it
  // internally, but there is no `rd_owner_gen_o` port -- and the island packs
  // a literal `8'd0` into the record's top eight bits where it would go. The
  // identity that would let a DOWNSTREAM stage notice a stale join exists as a
  // counter inside the bank and nowhere in the data path. Asserted here as a
  // documented absence so a future port addition has a test that changes.

  const int rc = zhao::report_and_exit("metajoin_seam_directed");
  delete d;
  zhao::exit_hard(rc);
}
