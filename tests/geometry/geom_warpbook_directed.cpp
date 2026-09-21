// geom_warpbook_directed.cpp -- GEOM.WARP's per-draw descriptor book
// (fpga/rtl/geometry/zhao_geom_warpbook.sv).
//
// WHAT THIS BLOCK IS FOR, in one sentence, because the assertions only make
// sense against it: directive 7.1 forbids copying DrawWarpedForm 0x0304's
// 456-bit snapshot through every geometry stage and requires "a compact
// descriptor cookie" on the draw item instead, so the snapshot lives here and
// six bits ride the vertex.
//
// THE CASE THAT MATTERS IS WB4. The book is a FOUR-ENTRY RING with no free
// list -- there is no signal in this console saying "the last vertex of draw N
// has passed the skinner", and inventing one from a vertex count would be a
// second implementation of a walk `zhao_geom_assetfetch` owns and would be
// wrong besides, because that block's `release_i` may truncate a meshlet's
// vertex service by design. So the fifth warped draw OVERWRITES the first, and
// the whole question is what a vertex of the first draw gets when it arrives
// after that. The answer must be W09's bypass and a counted refusal, never
// another draw's deformation -- which would be a plausible wrong SHAPE that
// nothing downstream can detect.
//
// EVERY COUNTER HERE IS REACHED BY LEGAL STIMULUS, so no mutant is owed
// (R95 owes one for a guard unreachable by legal input; five draws and a read
// are ordinary input). `stale_o` is discriminated in BOTH directions inside
// this file -- WB4 fires it on the overwritten cookie and asserts it stays
// STILL on the three cookies that survived, in the same trace.
#include <cstdint>
#include <cstdio>
#include "verilated.h"
#include "Vzhao_geom_warpbook.h"
#include "zhao_sim.hpp"

namespace {

using Dut = Vzhao_geom_warpbook;

void tick(Dut& d) { zhao::tick(d); }

void reset(Dut& d) {
  d.rst_n = 0;
  d.w_fire_i = 0;
  d.w_en_i = 0;
  d.r_fire_i = 0;
  d.r_cookie_i = 0;
  d.eval();
  tick(d);
  tick(d);
  d.rst_n = 1;
  d.eval();
  tick(d);
}

void set128(VlWide<4>& w, uint32_t a, uint32_t b, uint32_t c, uint32_t e) {
  w[0] = a; w[1] = b; w[2] = c; w[3] = e;
}

bool eq128(const VlWide<4>& w, uint32_t a, uint32_t b, uint32_t c, uint32_t e) {
  return w[0] == a && w[1] == b && w[2] == c && w[3] == e;
}

// Present one accepted draw and return the cookie the book issued for it.
// The cookie is COMBINATIONAL in the accept cycle -- it has to be, because
// `zhao_geom_drawjob` latches the whole draw on that edge and a cookie one
// clock later would belong to the draw after it. WB1 asserts exactly that.
uint8_t draw(Dut& d, bool en, uint32_t t, uint32_t p0, uint32_t attr0,
             uint8_t amode, int32_t bx, int32_t by, int32_t bz) {
  d.w_fire_i = 1;
  d.w_en_i = en ? 1 : 0;
  d.w_time_i = t;
  set128(d.w_par_i, p0, p0 + 1, p0 + 2, p0 + 3);
  set128(d.w_attr_i, attr0, attr0 + 1, attr0 + 2, attr0 + 3);
  d.w_attr_mode_i = amode;
  d.w_bx_i = static_cast<uint32_t>(bx);
  d.w_by_i = static_cast<uint32_t>(by);
  d.w_bz_i = static_cast<uint32_t>(bz);
  d.eval();
  const uint8_t ck = d.w_cookie_o;
  tick(d);
  d.w_fire_i = 0;
  d.w_en_i = 0;
  d.eval();
  return ck;
}

// Offer a cookie on the read port for one accepted vertex.
void read_vertex(Dut& d, uint8_t cookie) {
  d.r_cookie_i = cookie;
  d.r_fire_i = 1;
  d.eval();
}

void end_read(Dut& d) {
  tick(d);
  d.r_fire_i = 0;
  d.eval();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;
  using zhao::check;

  // =======================================================================
  // WB1. THE COOKIE IS COMBINATIONAL IN THE ACCEPT CYCLE, AND IT CARRIES
  // THE DRAW'S OWN ENABLE.
  // =======================================================================
  {
    reset(dut);
    check(dut.w_cookie_o == 0x00,
          "WB1: after reset the book offers stamp 0 with `en` CLEAR "
          "(nothing has asked for a Warp yet)", 0x00, dut.w_cookie_o);

    // A draw that names NO program. W09: that path performs zero Warp
    // lookups, so it must not consume a stamp -- burning one would shorten
    // the staleness detector's reach for nothing.
    dut.w_en_i = 0;
    dut.eval();
    check((dut.w_cookie_o >> 5) == 0,
          "WB1: an UNWARPED draw's cookie has `en` clear", 0, dut.w_cookie_o >> 5);
    const uint8_t plain = draw(dut, false, 0xDEAD, 0x10, 0x20, 0, 1, 1, 1);
    check(plain == 0x00, "WB1: and it is stamp 0 -- unchanged", 0x00, plain);

    dut.w_en_i = 1;
    dut.eval();
    check((dut.w_cookie_o >> 5) == 1,
          "WB1: a WARPED draw's cookie has `en` SET", 1, dut.w_cookie_o >> 5);
    const uint8_t a = draw(dut, true, 0x1111, 0x100, 0x200, 0, 10, 20, 30);
    check(a == 0x20, "WB1: the first warped draw is {en=1, stamp=0}", 0x20, a);
    check(dut.allocated_o == 1,
          "WB1: allocated_o FIRED on the warped draw and NOT on the plain one",
          1, dut.allocated_o);

    const uint8_t b = draw(dut, true, 0x2222, 0x300, 0x400, 1, -1, -2, -3);
    check(b == 0x21, "WB1: the second warped draw is {en=1, stamp=1}", 0x21, b);
    check(dut.allocated_o == 2, "WB1: allocated_o moved again", 2, dut.allocated_o);
  }

  // =======================================================================
  // WB2. A READ BY COOKIE RETURNS THAT DRAW'S WHOLE SNAPSHOT -- W05.
  // =======================================================================
  // W05 forbids a mutable global `current_warp` register because a later
  // setting "could retroactively change older draws". A single held register
  // would PASS a one-draw-at-a-time test, so this case holds TWO descriptors
  // live at once and reads the EARLIER one back after the later one landed.
  {
    reset(dut);
    const uint8_t a = draw(dut, true, 0x0000AAAA, 0x1000, 0x5000, 0, 100, 200, 300);
    const uint8_t b = draw(dut, true, 0x0000BBBB, 0x2000, 0x6000, 1, -400, -500, -600);
    check(a != b, "WB2: (premise) the two draws got DIFFERENT cookies", 1, a != b);

    read_vertex(dut, a);
    check(dut.r_en_o == 1, "WB2: draw A's cookie resolves", 1, dut.r_en_o);
    check(dut.r_time_o == 0x0000AAAAu,
          "WB2: and returns A's OWN tick, not the later draw's",
          0x0000AAAAu, dut.r_time_o);
    check(eq128(dut.r_par_o, 0x1000, 0x1001, 0x1002, 0x1003),
          "WB2: A's four parameter words", 1,
          eq128(dut.r_par_o, 0x1000, 0x1001, 0x1002, 0x1003));
    check(eq128(dut.r_attr_o, 0x5000, 0x5001, 0x5002, 0x5003),
          "WB2: A's four INLINE4 attribute words", 1,
          eq128(dut.r_attr_o, 0x5000, 0x5001, 0x5002, 0x5003));
    check(dut.r_attr_mode_o == 0, "WB2: A's attribute mode", 0, dut.r_attr_mode_o);
    check(static_cast<int32_t>(dut.r_bx_o) == 100 &&
          static_cast<int32_t>(dut.r_by_o) == 200 &&
          static_cast<int32_t>(dut.r_bz_o) == 300,
          "WB2: A's displacement bound, all three axes", 1,
          (static_cast<int32_t>(dut.r_bx_o) == 100 &&
           static_cast<int32_t>(dut.r_by_o) == 200 &&
           static_cast<int32_t>(dut.r_bz_o) == 300));
    end_read(dut);

    // The NEGATIVE half: the later draw is not the earlier one.
    read_vertex(dut, b);
    check(dut.r_time_o == 0x0000BBBBu,
          "WB2: draw B's cookie returns B's tick -- the book TRACKS the "
          "cookie rather than holding one global descriptor",
          0x0000BBBBu, dut.r_time_o);
    check(static_cast<int32_t>(dut.r_bx_o) == -400,
          "WB2: and B's NEGATIVE bound, sign preserved", -400,
          static_cast<int32_t>(dut.r_bx_o));
    check(dut.r_attr_mode_o == 1, "WB2: and B's attribute mode", 1,
          dut.r_attr_mode_o);
    end_read(dut);

    check(dut.hits_o == 2, "WB2: hits_o counted both accepted vertices", 2,
          dut.hits_o);
    check(dut.stale_o == 0, "WB2: and stale_o did NOT move", 0, dut.stale_o);
  }

  // =======================================================================
  // WB3. `en` LOW IS AN ORDINARY DRAW, AND IT IS COUNTED BY NEITHER SIDE.
  // =======================================================================
  {
    reset(dut);
    const uint8_t a = draw(dut, true, 0x4242, 0x900, 0xA00, 0, 7, 7, 7);
    const uint32_t h0 = dut.hits_o, s0 = dut.stale_o;

    // A cookie whose `en` is clear: entry 0 IS valid and its stamp DOES match,
    // so this is the case that separates "asked and resolved" from "did not
    // ask". A book that ignored the enable bit would answer r_en_o high here.
    read_vertex(dut, a & 0x1F);
    check(dut.r_en_o == 0,
          "WB3: a cookie with `en` CLEAR does not arm the Warp, even though "
          "its entry is valid and its stamp matches", 0, dut.r_en_o);
    end_read(dut);
    check(dut.hits_o == h0, "WB3: hits_o did not move", h0, dut.hits_o);
    check(dut.stale_o == s0,
          "WB3: and NEITHER did stale_o -- an unwarped draw is not a failed "
          "lookup, and counting it as one would drown the detector", s0,
          dut.stale_o);

    // The positive control in the same trace.
    read_vertex(dut, a);
    check(dut.r_en_o == 1, "WB3 POSITIVE CONTROL: the same cookie WITH `en` "
          "set does arm it", 1, dut.r_en_o);
    end_read(dut);
    check(dut.hits_o == h0 + 1, "WB3: and that one WAS counted", h0 + 1,
          dut.hits_o);
  }

  // =======================================================================
  // WB4. THE RING OVERWRITE IS DETECTED, NOT ANSWERED. This is the case the
  // block exists to survive.
  // =======================================================================
  {
    reset(dut);
    uint8_t ck[5];
    for (int i = 0; i < 5; ++i) {
      ck[i] = draw(dut, true, 0x1000u + static_cast<uint32_t>(i),
                   0x100u * (i + 1), 0x900u * (i + 1),
                   static_cast<uint8_t>(i & 1), i + 1, i + 2, i + 3);
    }
    check(dut.allocated_o == 5, "WB4: (setup) five warped draws allocated", 5,
          dut.allocated_o);
    // THE PREMISE. Draw 4 must land in draw 0's ENTRY while carrying a
    // DIFFERENT STAMP -- without both halves this case cannot discriminate.
    check((ck[4] & 0x03) == (ck[0] & 0x03),
          "WB4: (premise) draw 4 took draw 0's ENTRY -- the ring wrapped",
          ck[0] & 0x03, ck[4] & 0x03);
    check(ck[4] != ck[0],
          "WB4: (premise) but its STAMP differs -- the stamp is not a "
          "restatement of the entry index", 1, ck[4] != ck[0]);

    const uint32_t s0 = dut.stale_o, h0 = dut.hits_o;

    // A vertex of draw 0, arriving after draw 4 overwrote its entry.
    read_vertex(dut, ck[0]);
    check(dut.r_en_o == 0,
          "WB4: an OVERWRITTEN cookie does NOT arm the Warp -- it falls back "
          "to W09's bypass rather than deforming against draw 4's program",
          0, dut.r_en_o);
    end_read(dut);
    check(dut.stale_o == s0 + 1, "WB4: stale_o FIRED", s0 + 1, dut.stale_o);
    check(dut.hits_o == h0, "WB4: and hits_o did NOT", h0, dut.hits_o);

    // THE OTHER DIRECTION, in the same trace. The three cookies that survived
    // the wrap still resolve, and to their OWN descriptors -- so the detector
    // is not a counter that fires on everything.
    for (int i = 1; i <= 3; ++i) {
      const uint32_t sb = dut.stale_o, hb = dut.hits_o;
      read_vertex(dut, ck[i]);
      char msg[192];
      std::snprintf(msg, sizeof(msg),
                    "WB4 NEGATIVE CONTROL: surviving cookie %d still resolves", i);
      check(dut.r_en_o == 1, msg, 1, dut.r_en_o);
      std::snprintf(msg, sizeof(msg),
                    "WB4 NEGATIVE CONTROL: and returns draw %d's OWN tick", i);
      check(dut.r_time_o == 0x1000u + static_cast<uint32_t>(i), msg,
            0x1000u + static_cast<uint32_t>(i), dut.r_time_o);
      end_read(dut);
      check(dut.stale_o == sb, "WB4: stale_o stayed STILL on a live cookie",
            sb, dut.stale_o);
      check(dut.hits_o == hb + 1, "WB4: hits_o moved on it", hb + 1,
            dut.hits_o);
    }

    // And draw 4 itself, which now owns that entry.
    read_vertex(dut, ck[4]);
    check(dut.r_en_o == 1, "WB4: the draw that DID the overwriting resolves",
          1, dut.r_en_o);
    check(dut.r_time_o == 0x1004u,
          "WB4: to its own tick -- the entry holds draw 4, which is exactly "
          "why draw 0's vertex had to be refused", 0x1004u, dut.r_time_o);
    end_read(dut);
  }

  // =======================================================================
  // WB5. A COOKIE FOR AN ENTRY THAT WAS NEVER WRITTEN IS REFUSED.
  // =======================================================================
  // After reset nothing is valid, so every `en`-set cookie is stale. This is
  // the OTHER way the read can fail and it is a different mechanism from
  // WB4's: `vld_q` rather than the stamp compare. Both are asserted, because
  // a book that only checked the stamp would pass WB4 and answer an
  // uninitialised entry's zeroes here as a real descriptor.
  {
    reset(dut);
    for (uint8_t e = 0; e < 4; ++e) {
      read_vertex(dut, static_cast<uint8_t>(0x20 | e));
      char msg[160];
      std::snprintf(msg, sizeof(msg),
                    "WB5: entry %u was never written, so its cookie is REFUSED", e);
      check(dut.r_en_o == 0, msg, 0, dut.r_en_o);
      end_read(dut);
    }
    check(dut.stale_o == 4, "WB5: stale_o counted all four", 4, dut.stale_o);
    check(dut.hits_o == 0, "WB5: and hits_o stayed at zero", 0, dut.hits_o);

    // The negative control: write one and it resolves.
    const uint8_t a = draw(dut, true, 0x7777, 0x55, 0x66, 0, 5, 5, 5);
    read_vertex(dut, a);
    check(dut.r_en_o == 1,
          "WB5 NEGATIVE CONTROL: a written entry resolves, so the four "
          "refusals above are about `vld_q` and not about a dead read port",
          1, dut.r_en_o);
    end_read(dut);
  }

  // =======================================================================
  // WB6. THE COUNTERS ARE ON THE CONSUMER'S ACCEPT, NOT ON THE WIRE.
  // =======================================================================
  // The read is combinational and a cookie SITS on the port for as long as
  // the vertex it belongs to is being offered. A book that counted on the
  // wire would report stall cycles as vertices -- the "counters see what
  // pictures cannot" failure, inverted: a number several times too large,
  // and in the direction that flatters a throughput claim.
  {
    reset(dut);
    const uint8_t a = draw(dut, true, 0x8888, 0x11, 0x22, 0, 9, 9, 9);
    const uint8_t bad = static_cast<uint8_t>(0x20 | 0x03);  // entry 3, unwritten

    dut.r_cookie_i = a;
    dut.r_fire_i = 0;
    dut.eval();
    for (int i = 0; i < 20; ++i) tick(dut);
    check(dut.hits_o == 0,
          "WB6: twenty cycles with the cookie HELD and no accept counted "
          "NOTHING", 0, dut.hits_o);

    dut.r_cookie_i = bad;
    dut.eval();
    for (int i = 0; i < 20; ++i) tick(dut);
    check(dut.stale_o == 0,
          "WB6: and twenty more with a STALE cookie held counted nothing "
          "either", 0, dut.stale_o);

    read_vertex(dut, a);
    end_read(dut);
    check(dut.hits_o == 1, "WB6: one accept, one hit", 1, dut.hits_o);
    read_vertex(dut, bad);
    end_read(dut);
    check(dut.stale_o == 1, "WB6: one accept of a stale cookie, one stale", 1,
          dut.stale_o);
  }

  std::printf("geom_warpbook_directed: allocated=%u hits=%u stale=%u\n",
              static_cast<unsigned>(dut.allocated_o),
              static_cast<unsigned>(dut.hits_o),
              static_cast<unsigned>(dut.stale_o));
  return zhao::report_and_exit("geom_warpbook_directed");
}
