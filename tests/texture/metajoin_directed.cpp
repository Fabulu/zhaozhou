// metajoin_directed.cpp
//
// ---------------------------------------------------------------------------
// THE METADATA BANK, FIELD BY FIELD
// ---------------------------------------------------------------------------
// `zhao_texture_metajoin` is packet C's storage: 256 rows x 40 bits, one writer
// at planned-sample acceptance, one synchronous reader on the common response
// stream before it splits into class queues.
//
// WHY THIS TEST IS SHAPED THE WAY IT IS
// -------------------------------------
// The first version of that module hand-wrote its field offsets and ALL FIVE
// were off by one. It passed lint. Every output had a driver. Every field would
// have returned the wrong bits.
//
// A test that writes one record and checks "something came back" would have
// passed too. So every field here carries a DISTINCT value chosen so that an
// off-by-one shift cannot alias into a correct answer: no field is zero, no two
// adjacent fields share a value, and the fraction bytes differ from each other.
// That is the difference between checking that the bank stores something and
// checking that it stores the right thing in the right place.
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

struct Rec {
  uint8_t slot, sidx, ogen, pslot, pgen, fmt, fu, fv, bsel, nib;
};

void write_rec(Vzhao_texture_metajoin* d, const Rec& r) {
  d->wr_valid_i = 1;
  d->wr_slot_i = r.slot;
  d->wr_sidx_i = r.sidx;
  d->wr_owner_gen_i = r.ogen;
  d->wr_pal_slot_i = r.pslot;
  d->wr_pal_gen_i = r.pgen;
  d->wr_format_i = r.fmt;
  d->wr_frac_u_i = r.fu;
  d->wr_frac_v_i = r.fv;
  d->wr_byte_sel_i = r.bsel;
  d->wr_nibble_i = r.nib;
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

  // ---- every legal (slot, sidx), every field distinct ----------------------
  int field_errors = 0, latency_errors = 0, compared = 0;
  for (uint32_t slot = 0; slot < 64; ++slot) {
    for (uint32_t sidx = 0; sidx < 3; ++sidx) {  // 3 is illegal, tested below
      Rec r;
      r.slot = static_cast<uint8_t>(slot);
      r.sidx = static_cast<uint8_t>(sidx);
      // Distinct, non-zero, and chosen so a one-bit shift cannot alias.
      r.ogen = static_cast<uint8_t>(0x81u + slot);
      r.pslot = static_cast<uint8_t>((slot + sidx) & 3u);
      r.pgen = static_cast<uint8_t>(0x37u + sidx * 5u + slot);
      r.fmt = static_cast<uint8_t>(1u + ((slot + sidx) % 7u));  // never 0
      r.fu = static_cast<uint8_t>(0x5Au ^ (slot * 3u));
      r.fv = static_cast<uint8_t>(0xA5u ^ (slot * 7u + sidx));
      r.bsel = static_cast<uint8_t>((slot ^ sidx) & 1u);
      r.nib = static_cast<uint8_t>((slot + 1u) & 1u);
      write_rec(d, r);

      d->rd_valid_i = 1;
      d->rd_slot_i = r.slot;
      d->rd_sidx_i = r.sidx;
      d->rd_owner_gen_i = r.ogen;
      d->eval();
      // The result must NOT be available in the same cycle: this is a RAM.
      if (d->rd_result_valid_o) ++latency_errors;
      tick(d);
      d->rd_valid_i = 0;
      d->eval();

      if (!d->rd_result_valid_o) {
        ++latency_errors;
        continue;
      }
      ++compared;
      if (d->rd_pal_slot_o != r.pslot) ++field_errors;
      if (d->rd_pal_gen_o != r.pgen) ++field_errors;
      if (d->rd_format_o != r.fmt) ++field_errors;
      if (d->rd_frac_u_o != r.fu) ++field_errors;
      if (d->rd_frac_v_o != r.fv) ++field_errors;
      if (d->rd_byte_sel_o != r.bsel) ++field_errors;
      if (d->rd_nibble_o != r.nib) ++field_errors;
      tick(d);
    }
  }

  std::printf("  %d records written and read back | writes %u reads %u\n", compared, d->writes_o,
              d->reads_o);

  zhao::check(compared == 192, "all 192 legal (slot, sample_index) rows were exercised", 192,
              compared);
  zhao::check(field_errors == 0,
              "EVERY field reads back exactly what was written -- palette slot, "
              "palette generation, format, both fractions, byte select and "
              "nibble. The first version of this bank had all five hand-written "
              "offsets off by one and passed lint; distinct per-field values are "
              "what make that visible",
              0, field_errors);
  zhao::check(latency_errors == 0,
              "and the read is SYNCHRONOUS -- no result in the request cycle, a "
              "result in the next. That registered boundary is the entire point "
              "of moving the join here",
              0, latency_errors);

  // ---- sample index 3 is representable and refused --------------------------
  const uint32_t illegal_before = d->rd_illegal_sidx_o;
  const uint32_t reads_before = d->reads_o;
  d->rd_valid_i = 1;
  d->rd_slot_i = 5;
  d->rd_sidx_i = 3;
  d->rd_owner_gen_i = 0x81u + 5u;
  d->eval();
  tick(d);
  d->rd_valid_i = 0;
  d->eval();

  zhao::check(d->rd_illegal_sidx_o == illegal_before + 1,
              "a read at sample index 3 is COUNTED as illegal -- the address "
              "space encodes it and the sample protocol does not permit it",
              illegal_before + 1, d->rd_illegal_sidx_o);
  zhao::check(!d->rd_result_valid_o,
              "and returns NO result. An unwritten row is not a benign zero; it "
              "is whatever a previous owner left at that address",
              0, d->rd_result_valid_o ? 1 : 0);
  zhao::check(d->reads_o == reads_before, "and it is not counted as a read", reads_before,
              d->reads_o);

  // ---- the generation alignment check --------------------------------------
  Rec g;
  g.slot = 9;
  g.sidx = 1;
  g.ogen = 0x44;
  g.pslot = 2;
  g.pgen = 0x11;
  g.fmt = 5;
  g.fu = 0x3C;
  g.fv = 0xC3;
  g.bsel = 1;
  g.nib = 0;
  write_rec(d, g);
  const uint32_t mism_before = d->rd_gen_mismatch_o;

  d->rd_valid_i = 1;
  d->rd_slot_i = g.slot;
  d->rd_sidx_i = g.sidx;
  d->rd_owner_gen_i = 0x45;  // NOT the generation the row was written for
  d->eval();
  tick(d);
  d->rd_valid_i = 0;
  d->eval();
  tick(d);
  d->eval();

  zhao::check(d->rd_gen_mismatch_o == mism_before + 1,
              "a response naming a DIFFERENT generation than the row was written "
              "for is counted -- that is a slot recycled under a response still "
              "in flight, which is a real fault and not a filtering decision",
              mism_before + 1, d->rd_gen_mismatch_o);

  const int rc = zhao::report_and_exit("metajoin_directed");
  delete d;
  zhao::exit_hard(rc);
}
