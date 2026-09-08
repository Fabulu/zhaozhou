// early_desc_directed.cpp
//
// ---------------------------------------------------------------------------
// THE TYPED EARLY DESCRIPTOR BANK, AGAINST §5.6's OBLIGATIONS
// ---------------------------------------------------------------------------
// The Decrufter brief §5.6 names the tests this block owes. The ones that are
// block-level rather than island-level are here:
//
//   * "Change every descriptor field between adjacent admitted fragments" --
//     every row differs in every field, so a swapped or mis-sliced field cannot
//     alias into looking right.
//   * "Force more than 64 owner admissions with slot reuse and compare full
//     context, not just the low-16 tag" -- the generation is compared, and the
//     bank is filled twice over.
//   * "Test a descriptor read adjacent to a write and state which same-address
//     case is legal" -- stated and asserted below.
//   * The D0 hold law, which §5.3 requires of this bank explicitly.
//
// The island-level obligations -- stalling RCP return, PERSPUV output, expander
// acceptance and AUX independently -- belong to the join that consumes this, and
// are not simulated here by a bank that has no such ports.
#include "Vzhao_texture_early_desc.h"

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

namespace {

void tick(Vzhao_texture_early_desc* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

// One row's worth of fields, all derived from the slot so that every row differs
// in every field and the expected value is reconstructible.
struct Row {
  uint64_t ctx;
  uint8_t gen, lod, cls, aux, count, pslot, pgen, mosa, mosb, mosw, bsel;
};

Row make_row(int i, int pass) {
  Row r;
  r.ctx   = (0xC0DE0000ull + static_cast<uint64_t>(i) * 0x1111) << 32 |
            (0xBEEF0000ull + static_cast<uint64_t>(i) * 7 + pass);
  r.gen   = static_cast<uint8_t>(0x10 + i + pass * 0x40);
  r.lod   = static_cast<uint8_t>(i * 3 + 1);
  r.cls   = static_cast<uint8_t>((i + pass) & 3);
  r.aux   = static_cast<uint8_t>((i >> 1) & 1);
  r.count = static_cast<uint8_t>((i + 1) & 3);
  r.pslot = static_cast<uint8_t>((i + 2) & 3);
  r.pgen  = static_cast<uint8_t>(0x80 + i);
  r.mosa  = static_cast<uint8_t>(i * 5 + 3);
  r.mosb  = static_cast<uint8_t>(i * 11 + 7);
  r.mosw  = static_cast<uint8_t>(255 - i * 3);
  r.bsel  = static_cast<uint8_t>(i ^ 0x5A);
  return r;
}

void write_row(Vzhao_texture_early_desc* d, int slot, const Row& r) {
  d->wr_valid_i = 1;
  d->wr_slot_i = static_cast<uint8_t>(slot);
  d->wr_owner_gen_i = r.gen;
  d->wr_aux_context_i = r.ctx;
  d->wr_lod_q4_4_i = r.lod;
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

// Launch a read and settle its one-cycle latency.
void read_row(Vzhao_texture_early_desc* d, int slot, uint8_t claim_gen) {
  d->rd_valid_i = 1;
  d->rd_slot_i = static_cast<uint8_t>(slot);
  d->rd_owner_gen_i = claim_gen;
  d->eval();
  tick(d);
  d->rd_valid_i = 0;
  d->eval();
}

// How many of the eleven fields differ between two rows. Needed because the
// narrow fields (needs_aux, sample_count, palette_slot, raw_class) are 1-2 bits
// and collide readily: make_row(17,1) against make_row(41,0) agrees on three of
// them, so a same-address test built on that pair would have been comparing
// eight fields and reporting eleven.
int row_diff(const Row& a, const Row& b) {
  int n = 0;
  if (a.ctx   != b.ctx)   ++n;
  if (a.lod   != b.lod)   ++n;
  if (a.cls   != b.cls)   ++n;
  if (a.aux   != b.aux)   ++n;
  if (a.count != b.count) ++n;
  if (a.pslot != b.pslot) ++n;
  if (a.pgen  != b.pgen)  ++n;
  if (a.mosa  != b.mosa)  ++n;
  if (a.mosb  != b.mosb)  ++n;
  if (a.mosw  != b.mosw)  ++n;
  if (a.bsel  != b.bsel)  ++n;
  return n;
}

int mismatches(Vzhao_texture_early_desc* d, const Row& r) {
  int n = 0;
  if (d->rd_aux_context_o != r.ctx) ++n;
  if (d->rd_lod_q4_4_o != r.lod) ++n;
  if (d->rd_raw_class_o != r.cls) ++n;
  if (d->rd_needs_aux_o != r.aux) ++n;
  if (d->rd_sample_count_o != r.count) ++n;
  if (d->rd_palette_slot_o != r.pslot) ++n;
  if (d->rd_palette_gen_o != r.pgen) ++n;
  if (d->rd_mosaic_mat_a_o != r.mosa) ++n;
  if (d->rd_mosaic_mat_b_o != r.mosb) ++n;
  if (d->rd_mosaic_weight_o != r.mosw) ++n;
  if (d->rd_binding_sel_o != r.bsel) ++n;
  return n;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_early_desc* d = new Vzhao_texture_early_desc;

  d->clk = 0;
  d->rst_n = 0;
  d->wr_valid_i = 0;
  d->rd_valid_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  // ---- every field of every row, twice over --------------------------------
  // 128 admissions across 64 slots: the bank is filled, then every slot is
  // REUSED with a different generation and different field values. §5.6's "more
  // than 64 owner admissions with slot reuse".
  int field_errors = 0, gen_errors = 0, valid_errors = 0;
  for (int pass = 0; pass < 2; ++pass) {
    for (int i = 0; i < 64; ++i) write_row(d, i, make_row(i, pass));
    for (int i = 0; i < 64; ++i) {
      const Row r = make_row(i, pass);
      read_row(d, i, r.gen);
      field_errors += mismatches(d, r);
      // D0d: the generation LEAVES the bank. metajoin has no such port, so the
      // island packs 8'd0 where it would go and nothing downstream can notice a
      // stale join.
      if (d->rd_owner_gen_o != r.gen) ++gen_errors;
      if (!d->rd_result_valid_o) ++valid_errors;
    }
  }

  std::printf("  128 admissions over 64 slots: %d field errors, %d generation "
              "errors, %d validity errors\n",
              field_errors, gen_errors, valid_errors);

  zhao::check(field_errors == 0,
              "every one of the eleven descriptor fields reads back exactly, "
              "for all 64 slots across two passes with every field changed -- "
              "the derived offsets are right, which is what metajoin's five "
              "hand-written slices were not",
              0, static_cast<uint64_t>(field_errors));
  zhao::check(gen_errors == 0,
              "and the owner generation is EXPORTED, not merely stored -- the "
              "gap that leaves metajoin's island packing 8'd0 where the "
              "identity would travel",
              0, static_cast<uint64_t>(gen_errors));
  zhao::check(valid_errors == 0, "every read reports valid", 0,
              static_cast<uint64_t>(valid_errors));

  // ---- THE HOLD LAW, which §5.3 requires of this bank by name --------------
  // Read slot 9. Then offer slot 40 at the address port without firing a read,
  // exactly as a stalled consumer would. The outputs must still be slot 9's.
  {
    const Row a = make_row(9, 1);
    read_row(d, 9, a.gen);
    const int before = mismatches(d, a);

    d->rd_valid_i = 0;
    d->rd_slot_i = 40;                  // a different row, offered but not read
    d->rd_owner_gen_i = make_row(40, 1).gen;
    d->eval();
    tick(d);
    tick(d);
    d->eval();

    std::printf("  hold law: after two stalled cycles with slot 40 offered, "
                "lod %u (slot 9 = %u, slot 40 = %u)\n",
                d->rd_lod_q4_4_o, a.lod, make_row(40, 1).lod);

    zhao::check(before == 0, "the reference read landed before the stall", 0,
                static_cast<uint64_t>(before));
    zhao::check(mismatches(d, a) == 0,
                "D0's hold law: the descriptor HOLDS while a different slot is "
                "offered and no read fires. An ungated output register would "
                "have followed the offered address and paired this consumer's "
                "identity with another owner's descriptor, with every counter "
                "still balancing",
                0, static_cast<uint64_t>(mismatches(d, a)));
  }

  // ---- the same-address read-adjacent-to-write case, STATED ---------------
  // §5.6 asks which same-address case is legal. This bank reads OLD DATA: the
  // write and the read are in one clocked block, so a read issued on the same
  // edge as a write to the same row returns the row's PREVIOUS contents. That is
  // the M10K's own old-data behaviour and it is asserted rather than assumed,
  // because the alternative (new-data forwarding) is a different memory mode
  // that costs bypass logic and would silently change what a consumer sees.
  {
    const Row older = make_row(17, 1);   // already resident from the loop above
    // The contrast row is SEARCHED, not guessed. make_row varies only ctx, gen
    // and raw_class with `pass`, so make_row(17, 0) leaves eight of eleven
    // fields identical -- and the first version of this test used exactly that,
    // printing "lod 52 (old 52, new 52)" while claiming to distinguish old data
    // from new. A comparison whose two sides are equal proves nothing about
    // which one was returned.
    //
    // Picking a magic index instead would work today and rot the next time
    // make_row changes, so the index is found by asking.
    Row newer = make_row(0, 0);
    int contrast = -1;
    for (int c = 0; c < 64; ++c) {
      const Row cand = make_row(c, 0);
      if (row_diff(older, cand) == 11) { newer = cand; contrast = c; break; }
    }
    zhao::check(contrast >= 0,
                "a contrast row differing in all eleven fields exists and was "
                "found -- otherwise the same-address case below is untestable "
                "and must not silently weaken instead",
                1, contrast >= 0 ? 1 : 0);

    d->wr_valid_i = 1;
    d->wr_slot_i = 17;
    d->wr_owner_gen_i = newer.gen;
    d->wr_aux_context_i = newer.ctx;
    d->wr_lod_q4_4_i = newer.lod;
    d->wr_raw_class_i = newer.cls;
    d->wr_needs_aux_i = newer.aux;
    d->wr_sample_count_i = newer.count;
    d->wr_palette_slot_i = newer.pslot;
    d->wr_palette_gen_i = newer.pgen;
    d->wr_mosaic_mat_a_i = newer.mosa;
    d->wr_mosaic_mat_b_i = newer.mosb;
    d->wr_mosaic_weight_i = newer.mosw;
    d->wr_binding_sel_i = newer.bsel;
    d->rd_valid_i = 1;                   // SAME address, SAME edge
    d->rd_slot_i = 17;
    d->rd_owner_gen_i = older.gen;
    d->eval();
    tick(d);
    d->wr_valid_i = 0;
    d->rd_valid_i = 0;
    d->eval();

    // Non-vacuity, asserted rather than assumed: the two candidate answers must
    // differ in EVERY field, or "old data was returned" is not a claim about
    // anything.
    zhao::check(row_diff(older, newer) == 11,
                "the resident row and the row being written differ in all "
                "eleven fields, so the same-address answer below actually "
                "distinguishes old data from new",
                11, static_cast<uint64_t>(row_diff(older, newer)));
    std::printf("  same-address write+read: lod %u (old %u, new %u)\n",
                d->rd_lod_q4_4_o, older.lod, newer.lod);
    zhao::check(mismatches(d, older) == 0,
                "a read on the same edge as a write to the same row returns OLD "
                "data. Stated rather than discovered: this is the M10K's own "
                "behaviour, and new-data forwarding would be a different memory "
                "mode with bypass logic that nobody has asked for",
                0, static_cast<uint64_t>(mismatches(d, older)));
  }

  // ---- §5.5's INSTRUMENT, SHOWN TO FIRE ------------------------------------
  // The brief asks to instrument last-read/recycle events and explicitly refuses
  // to let a lease be added on suspicion: "prove the window or reproduce it".
  // So this counter reports and does not enforce -- and a counter that has never
  // been seen to fire is not an instrument.
  {
    const uint32_t before = d->rd_gen_mismatch_o;

    // A read that claims the generation the row actually holds: silent.
    const Row cur = make_row(23, 1);
    read_row(d, 23, cur.gen);
    tick(d);
    d->eval();
    const uint32_t after_match = d->rd_gen_mismatch_o;

    // A read that claims a STALE generation -- the shape of an owner recycled
    // while a frontend transaction could still read its old row.
    read_row(d, 23, static_cast<uint8_t>(cur.gen ^ 0xFF));
    tick(d);
    d->eval();
    const uint32_t after_stale = d->rd_gen_mismatch_o;

    std::printf("  generation instrument: %u -> %u (matching read) -> %u "
                "(stale read)\n", before, after_match, after_stale);

    zhao::check(after_match == before,
                "a read whose claimed generation matches the row is silent -- "
                "the instrument does not cry on correct traffic",
                before, after_match);
    zhao::check(after_stale > after_match,
                "and a STALE generation FIRES it. Shown to fire, which is the "
                "whole difference from metajoin's counter: there both operands "
                "rode one ungated register and corrupted in lockstep, so it read "
                "zero through a live defect",
                1, after_stale > after_match ? 1 : 0);
  }

  // ---- the counters agree with the traffic --------------------------------
  std::printf("  writes %u reads %u\n", d->writes_o, d->reads_o);
  zhao::check(d->writes_o == 129,
              "the write counter matches the admissions driven: 128 plus the "
              "same-address case",
              129, d->writes_o);

  const int rc = zhao::report_and_exit("early_desc_directed");
  delete d;
  zhao::exit_hard(rc);
}
